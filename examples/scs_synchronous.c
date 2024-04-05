/*
This file contains an example implementation for the scheduling of the Clocks FMU. 
Although the Scheduled Execution interface is used, all model partitions are 
executed in the same thread. The model partitions are executed in the order determined by the priorities 
of the clocks, i.e. the execution of a model partition is not interrupted by the execution of a model 
partition with higher priority. 

Overview of the clocks:

    time        0 1 2 3 4 5 6 7 8 9
    inClock1    + + + + + + + + + +
    inClock2    + +             + +
    inClock3            +          
    outClock    ? ? ? ? ? ? ? ? ? ?
    time        0 1 2 3 4 5 6 7 8 9

    inClock1:   Constant input clock. Activated by the simulation algorithm every second 
                according to the specification of Clock1 in the FMU's ModelDescription.xml
    inClock2:   Triggered input clock. Activated by the simulation algorithm at 0sec, 1sec, 8sec and 9sec. 
                The activation times are set by the simulator, not by the FMU.
    inClock3:   Countdown clock. Clock tick set by the FMU in model partition 2 at 4sec.
    outClock:   Output clock. Clock tick set by all input clocks every 5th time one of the input clocks 
                ticks(i.e. variable totalInTicks % 5 == 0).

In this example, output3 (belonging to Clock3) is connected to input2 (belonging to Clock2). Note that it
is rather unusual to connect input and output of the same FMU, but in this example we would like to show 
the effects of task interruptions with only one FMU.
The simulator activates Clock3 only once at time 4sec. In ModelPartition3, output3 is set to 1000. As the 
simulation takes place without preemption, this new value is not available in the next time step. 
 */

#define OUTPUT_FILE  "scs_synchronous_out.csv"
#define LOG_FILE     "scs_synchronous_log.txt"

#include "util.h"


/* *****************  Input clocks ***************** */
fmi3Float64 countdownClockIntervals[1] = { 0.0 };
fmi3IntervalQualifier countdownClocksQualifier[1] = { fmi3IntervalNotYetKnown };
fmi3ValueReference vr_countdownClocks[1] = { vr_inClock3 };


/* *****************  Output clocks ***************** */
fmi3Clock outputClocks[1] = { fmi3ClockInactive };
const fmi3ValueReference vrOutputClocks[1] = { vr_outClock };


/* *****************  Inputs ***************** */
fmi3Int32 inputs_c2[1] = { 0 };
const fmi3ValueReference vrInputs_c2[1] = { vr_input2 };

/* *****************  Outputs ***************** */
fmi3Int32 outputs_c1[3] = { 0 };
fmi3Int32 outputs_c2[3] = { 0 };
fmi3Int32 outputs_c3[3] = { 0 };
const fmi3ValueReference vrOutputs_c1[2] = { vr_inClock1Ticks, vr_totalInClockTicks };
const fmi3ValueReference vrOutputs_c2[3] = { vr_inClock2Ticks , vr_totalInClockTicks, vr_result2 };
const fmi3ValueReference vrOutputs_c3[3] = { vr_inClock3Ticks, vr_totalInClockTicks, vr_output3 };


/*
 * cb_clockUpdate()
*/
static void cb_clockUpdate(fmi3InstanceEnvironment instanceEnvironment) {
    // Check if countdown clock has ticked 
    if (countdownClocksQualifier != fmi3IntervalChanged){
        countdownClockIntervals[0] = 0.0;
        CALL(FMI3GetIntervalDecimal(S, vr_countdownClocks, 1, countdownClockIntervals, countdownClocksQualifier));
    }

    // Check if output clock has ticked 
    if (outputClocks[0] == fmi3ClockInactive) {
        CALL(FMI3GetClock(S, vrOutputClocks, 1, outputClocks));
    }
}

/*
cb_lockPreemption()
This scheduler does not use preemption, i.e. no locking necessary.
*/
void cb_lockPreemption(void) {
    // Nothing to be done. 
}
/*
cb_unlockPreemption()
This scheduler does not use preemption, i.e. no locking necessary.
*/
void cb_unlockPreemption(void) {
    // Nothing to be done. 
}


int main(int argc, char* argv[]) {

    CALL(setUp());

    CALL(FMI3InstantiateScheduledExecution(S,
        INSTANTIATION_TOKEN,   // instantiationToken
        NULL,                  // resourcePath
        fmi3False,             // visible
        fmi3False,             // loggingOn
        NULL,                  // instanceEnvironment
        NULL,                  // logMessage
        cb_clockUpdate,        // clockUpdate
        cb_lockPreemption,     // lockPreemption
        cb_unlockPreemption    // unlockPreemption
    ));


    CALL(FMI3EnterInitializationMode(S, fmi3False, 0, 0, fmi3False, 0));
    CALL(FMI3ExitInitializationMode(S));

    int time = 0;

    // simulation loop
    while (time < 10) {

        // Model Partition 1 is active every second
        CALL(FMI3ActivateModelPartition(S, vr_inClock1, time));
        CALL(FMI3GetInt32(S, vrOutputs_c1, 2, outputs_c1, 2));

        // Model Partition 2 is active at 0, 1, 8, and 9
        if (time % 8 == 0 || (time - 1) % 8 == 0) {
            CALL(FMI3SetInt32(S, vrInputs_c2, 1, inputs_c2, 1));
            CALL(FMI3ActivateModelPartition(S, vr_inClock2, time));
            CALL(FMI3GetInt32(S, vrOutputs_c2, 3, outputs_c2, 3));
        }

        if (countdownClocksQualifier[0] == fmi3IntervalChanged) {
            CALL(FMI3ActivateModelPartition(S, vr_inClock3, time));
            countdownClocksQualifier[0] = fmi3IntervalUnchanged;
            CALL(FMI3GetInt32(S, vrOutputs_c3, 3, outputs_c3, 3));
            // Use the output of model partition 3 as input for model partition 2
            inputs_c2[0] = outputs_c3[2];
        }

        if (outputClocks[0] == fmi3ClockActive) {
           //  If output clock is connected to another input clock, the input clock
           //  must be activated. In this example, only a log message is created.
           logEvent("Detected ticking of output clock (time=%g)", time);
           outputClocks[0] = fmi3ClockInactive;
        }

        CALL(recordVariables(S, time, outputFile));

        time++;
    }

TERMINATE:
    return tearDown();
}
