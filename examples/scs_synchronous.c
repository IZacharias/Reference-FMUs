#define OUTPUT_FILE  "scs_synchronous_out.csv"
#define LOG_FILE     "scs_synchronous_log.txt"

#include "util.h"

/* *****************  Inputs ***************** */
fmi3Int32 inputs_c2[1] = { 0 };
const fmi3ValueReference vrInputs_c2[1] = { vr_input2 };


/* *****************  Input clocks ***************** */
fmi3Float64 countdownClockIntervals[1] = { 0.0 };
fmi3IntervalQualifier countdownClocksQualifier[1] = { fmi3IntervalNotYetKnown };
fmi3ValueReference vr_countdownClocks[1] = { vr_inClock3 };
fmi3ValueReference outClockVRs[1] = { vr_outClock };
fmi3Clock outClockValues[2];

/* *****************  Outputs ***************** */
// separate the output variables after their clock (== modelpartition)
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
    countdownClockIntervals[0] = 0.0;
    countdownClocksQualifier[0] = fmi3IntervalNotYetKnown;
    FMI3GetIntervalDecimal(S, vr_countdownClocks, 1, countdownClockIntervals, countdownClocksQualifier);
}


/*
 * cb_lockPreemption()
 * Callback function to grab a lock in order to avoid preemption in a critical section
 * Works on a globally defined variable initialized by the importer
*/
void cb_lockPreemption(void) {
    // Nothing to be done. 
}
/*
 * cb_unlockPreemption()
 * Callback function to release a lock
 * Works on a globally defined variable initialized by the importer
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

        CALL(FMI3GetClock(S, outClockVRs, 1, outClockValues));

        CALL(recordVariables(S, time, outputFile));

        time++;
    }

TERMINATE:
    return tearDown();
}
