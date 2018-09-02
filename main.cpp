
#include "ifttt/WebHookSession.h"
#include "ifttt/WebHookEvent.h"
#include "ifttt/PowerSwitch.h"
#include "automation/Automation.h"
#include "automation/constraint/NotConstraint.h"
#include "automation/constraint/AndConstraint.h"
#include "automation/constraint/OrConstraint.h"
#include "automation/constraint/BooleanConstraint.h"
#include "automation/constraint/ValueConstraint.h"
#include "automation/constraint/ToggleConstraint.h"
#include "automation/constraint/SimultaneousConstraint.h"
#include "automation/constraint/TimeRangeConstraint.h"
#include "automation/constraint/TransitionDurationConstraint.h"
#include "automation/device/Device.h"
#include "automation/device/MutualExclusionDevice.h"

#include "Prometheus.h"

#include "Poco/Util/Application.h"

#include <iostream>
#include <numeric>

#define MINUTES 60000
#define SECONDS 1000

using namespace std;
using namespace automation;
using namespace ifttt;

class IftttApp : public Poco::Util::Application {

public:
  virtual int main(const std::vector<std::string> &args) {

    auto metricFilter = [](const Prometheus::Metric &metric) { return metric.name.find("solar") == 0; };

    static Prometheus::DataSource prometheusDs(Prometheus::URL, metricFilter);

    static SensorFn soc("State of Charge", []()->float{ return prometheusDs.metrics["solar_charger_batterySOC"].avg(); });
    static SensorFn generatedPower("Generated Power from Chargers",
                                 []()->float{ return prometheusDs.metrics["solar_charger_outputPower"].total(); });
    static SensorFn batteryBankVoltage("Average Battery Terminal Voltage from Chargers",
                                     []()->float{ return prometheusDs.metrics["solar_charger_outputVoltage"].total(); });

    static vector<automation::PowerSwitch *> devices;

    // If 500 watts should be allocated to each switch (air conditioner), this scale function
    // will be equivalent to checking for 1000 watts if 2 are running or 1500 for 3.
    auto scaleFn = [](float sourceVal)->float{
      long cnt = count_if(devices.begin(),devices.end(),[](automation::PowerSwitch* s){ return s->isOn(); });
      return sourceVal/(1+cnt);
    };
    static ScaledSensor scaledGeneratedPower(generatedPower,scaleFn);

    static struct FamilyRoomMasterSwitch : ifttt::PowerSwitch {

      MinConstraint<float,Sensor> minSoc {30, soc};
      MinConstraint<float,Sensor> minGeneratedWatts {400, scaledGeneratedPower};
      AndConstraint enoughPower {{&minSoc, &minGeneratedWatts}};
      MinConstraint<float,Sensor> fullSoc {100, soc};
      OrConstraint fullSocOrEnoughPower {{&fullSoc, &enoughPower}};
      TimeRangeConstraint timeRange { {9,30,0},{17,30,00} };
      SimultaneousConstraint simultaneousToggleOn {60*SECONDS,&toggle};
      NotConstraint notSimultaneousToggleOn {&simultaneousToggleOn};
      TransitionDurationConstraint minOffDuration{5*MINUTES,&toggle,0,1};
      AndConstraint familyRmMasterConstraints {{&minOffDuration,&timeRange,&notSimultaneousToggleOn,&fullSocOrEnoughPower}};

      FamilyRoomMasterSwitch() :
          ifttt::PowerSwitch("Family Room Master") {
        setOnEventLabel("family_room_master_switch_on");
        setOffEventLabel("family_room_master_switch_off");
        fullSoc.setPassDelayMs(1*MINUTES).setFailDelayMs(30*SECONDS).setFailMargin(55);
        minSoc.setPassDelayMs(1*MINUTES).setFailDelayMs(30*SECONDS).setFailMargin(15).setPassMargin(45);
        minGeneratedWatts.setPassDelayMs(1*MINUTES).setFailDelayMs(1*MINUTES).setFailMargin(200);
        //familyRmMasterConstraints.setPassDelayMs(5*MINUTES);
        pConstraint = &familyRmMasterConstraints;
      }
    } familyRoomMasterSwitch;

    static ToggleStateConstraint familyRoomMasterMustBeOn(&familyRoomMasterSwitch.toggle);
    familyRoomMasterMustBeOn.setPassDelayMs(3*MINUTES);

    static struct FamilyRoomAuxSwitch : ifttt::PowerSwitch {

      MinConstraint<float,Sensor> minSoc {70, soc};
      MinConstraint<float,Sensor> minGeneratedWatts {475, scaledGeneratedPower};
      AndConstraint enoughPower {{&minSoc, &minGeneratedWatts}};
      TimeRangeConstraint timeRange { {12,0,0},{16,0,0} };
      SimultaneousConstraint simultaneousToggleOn {60*SECONDS,&toggle};
      NotConstraint notSimultaneousToggleOn {&simultaneousToggleOn};
      TransitionDurationConstraint minOffDuration{20*MINUTES,&toggle,0,1};
      AndConstraint familyRmAuxConstraints {{&minOffDuration,&timeRange,&notSimultaneousToggleOn,&enoughPower}};

      FamilyRoomAuxSwitch() :
          ifttt::PowerSwitch("Family Room Auxiliary") {
        setOnEventLabel("family_room_aux_switch_on");
        setOffEventLabel("family_room_aux_switch_off");
        minSoc.setPassDelayMs(3*MINUTES).setFailDelayMs(10*SECONDS).setFailMargin(50).setPassMargin(5);
        minGeneratedWatts.setPassDelayMs(3*MINUTES).setFailDelayMs(10*SECONDS).setFailMargin(350);
        pPrerequisiteConstraint = &familyRoomMasterMustBeOn;
        pConstraint = &familyRmAuxConstraints;
      }
    } familyRoomAuxSwitch;


    static struct SunroomMasterSwitch : ifttt::PowerSwitch {

      MinConstraint<float,Sensor> minSoc {30, soc};
      MinConstraint<float,Sensor> minGeneratedWatts {400, scaledGeneratedPower};
      AndConstraint enoughPower {{&minSoc, &minGeneratedWatts}};
      MinConstraint<float,Sensor> fullSoc {100, soc};
      OrConstraint fullSocOrEnoughPower {{&fullSoc, &enoughPower}};
      TimeRangeConstraint timeRange { {8,30,0},{17,00,00} };
      SimultaneousConstraint simultaneousToggleOn {60*SECONDS,&toggle};
      NotConstraint notSimultaneousToggleOn {&simultaneousToggleOn};
      TransitionDurationConstraint minOffDuration{5*MINUTES,&toggle,0,1};
      AndConstraint sunroomMasterConstraints {{&minOffDuration,&timeRange,&notSimultaneousToggleOn,&fullSocOrEnoughPower}};

      SunroomMasterSwitch() :
          ifttt::PowerSwitch("Sunroom Master") {
        setOnEventLabel("sunroom_master_switch_on");
        setOffEventLabel("sunroom_master_switch_off");
        fullSoc.setPassDelayMs(1*MINUTES).setFailDelayMs(20*SECONDS).setFailMargin(55);
        minSoc.setPassDelayMs(1*MINUTES).setFailDelayMs(20*SECONDS).setFailMargin(15).setPassMargin(45);
        minGeneratedWatts.setPassDelayMs(1*MINUTES).setFailDelayMs(20*SECONDS).setFailMargin(200);
        pConstraint = &sunroomMasterConstraints;
      }
    } sunroomMasterSwitch;

    //MutualExclusionDevice mutualExclusionDevice("Secondary Air Conditioners Group",{&sunroomMasterSwitch,&familyRoomAuxSwitch});

    devices = {&familyRoomMasterSwitch, &sunroomMasterSwitch, &familyRoomAuxSwitch}; //mutualExclusionDevice};

    // SimultaneousConstraint needs to be a listener of each capability it will watch as a group
    familyRoomMasterSwitch.simultaneousToggleOn.listen(&familyRoomAuxSwitch.toggle).listen(&sunroomMasterSwitch.toggle);
    familyRoomAuxSwitch.simultaneousToggleOn.listen(&familyRoomMasterSwitch.toggle).listen(&sunroomMasterSwitch.toggle);
    sunroomMasterSwitch.simultaneousToggleOn.listen(&familyRoomMasterSwitch.toggle).listen(&familyRoomAuxSwitch.toggle);

    unsigned long syncTimeMs = 0;

    bool firstTime = true;

    TimeRangeConstraint solarTimeRange({0,0,0},{17,30,0}); // app exits at 5:30pm each day

    while ( solarTimeRange.test() ) {

      prometheusDs.loadMetrics();

      unsigned long nowMs = automation::millisecs();
      unsigned long elapsedSyncTimeMs = nowMs - syncTimeMs;

      if ( elapsedSyncTimeMs > 20 * MINUTES ) {
        syncTimeMs = nowMs;
        cout << ">>>>> Synchronizing current state (sending to IFTTTT) <<<<<<" << endl;
        automation::bSynchronizing = !firstTime;
        firstTime = false;
      }

      bool bIgnoreSameState = syncTimeMs != nowMs; // don't send results to ifttt unless state change or time to sync

      for (Device *pDevice : devices) {

        automation::logBuffer.str("");
        automation::logBuffer.clear();
        pDevice->applyConstraint(bIgnoreSameState);
        string strLogBuffer = automation::logBuffer.str();

        if ( !strLogBuffer.empty() ) {
          cout << "=====================================================================" << endl;
          cout << "DEVICE: " << pDevice->name << endl;
          cout << strLogBuffer;
          cout << "TIME: " << DateTimeFormatter::format(LocalDateTime(), DateTimeFormat::SORTABLE_FORMAT) << endl;
        }
      }

      automation::bSynchronizing = false;
      automation::sleep(1000);
    };

    return 0;
  }
};

POCO_APP_MAIN(IftttApp);
