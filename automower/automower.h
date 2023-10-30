#include "esphome.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <list>




class Automower : public PollingComponent, public UARTDevice {

private:
    const uint8_t MAN_DATA[5] = {0x0F, 0x81, 0x2C, 0x00, 0x00};
    const uint8_t AUTO_DATA[5] = {0x0F, 0x81, 0x2C, 0x00, 0x01};
    const uint8_t HOME_DATA[5] = {0x0F, 0x81, 0x2C, 0x00, 0x03};
    const uint8_t DEMO_DATA[5] = {0x0F, 0x81, 0x2C, 0x00, 0x04};
    const uint8_t getModeCmd[5] = {0x0F, 0x01, 0x2C, 0x00, 0x00};
    const uint8_t getStatusCode[5] = {0x0F, 0x01, 0xF1, 0x00, 0x00};
    const uint8_t getChargingTime[5] = {0x0F, 0x01, 0xEC, 0x00, 0x00};


    const uint8_t getBatteryCurrent[5] = {0x0F, 0x01, 0xEB, 0x00, 0x00}; //mA
    
    const uint8_t getBatteryLevel[5] = {0x0F, 0x01, 0xEF, 0x00, 0x00}; //BatteryCapacityMah 

    const uint8_t getBatteryCapacityAtSearchStart[5] = {0x0F, 0x01, 0xF0, 0x00, 0x00};
    const uint8_t getBatteryUsed[5] = {0x0F, 0x2E, 0xE0, 0x00, 0x00};

    const uint8_t getBatteryVoltage[5] = {0x0F, 0x2E, 0xF4, 0x00, 0x00};//Battery voltage [mV]

    const uint8_t getFirmwareVersion[5] = {0x0F, 0x33, 0x90, 0x00, 0x00};
    

    //const uint8_t* polledCommands[2] = {getModeCmd,getStatusCode};

    //std::vector<const uint8_t *> commandVector = {getModeCmd,getStatusCode};
    const std::list<const uint8_t *> pollingCommandList = {
        getModeCmd,
        getStatusCode,
        getBatteryLevel,
        getChargingTime,
        getBatteryUsed,
        getBatteryVoltage,
        getFirmwareVersion,
        };


    bool _writable = true;


public:


    Sensor *batteryLevelSensor = new Sensor();
    Sensor *batteryUsedSensor = new Sensor();
    Sensor *chargingTimeSensor = new Sensor();
    Sensor *batteryVoltageSensor = new Sensor();
    Sensor *firmwareVersionSensor = new Sensor();
    TextSensor *modeTextSensor = new TextSensor();
    TextSensor *statusTextSensor = new TextSensor();
    TextSensor *lastCodeReceivedTextSensor = new TextSensor();
    

    Automower(UARTComponent *parent,uint32_t update_interval) : 
    PollingComponent(update_interval), UARTDevice(parent) {

        //std::vector<uint8_t*> commandVector = new std::vector<uint8_t*>();
        //(polledCommands, polledCommands + sizeof(polledCommands) / sizeof(polledCommands[0]) );
        //pollingCommandList.push_back(getModeCmd);
        //pollingCommandList.push_back(getStatusCode);

        batteryLevelSensor->set_accuracy_decimals(0);
        //set_state_class
        chargingTimeSensor->set_accuracy_decimals(0);
        batteryUsedSensor->set_accuracy_decimals(0);
        batteryVoltageSensor->set_accuracy_decimals(3);
        firmwareVersionSensor->set_accuracy_decimals(0);


    }


    void setMode(const std::string &value) {
       if (value == "MAN") {
            write_array(MAN_DATA, sizeof(MAN_DATA));
        } else if (value == "AUTO") {
            write_array(AUTO_DATA, sizeof(AUTO_DATA));
        } else if (value == "HOME") {
            write_array(HOME_DATA, sizeof(HOME_DATA));
        } else if (value == "DEMO") {
            write_array(DEMO_DATA, sizeof(DEMO_DATA));
        } else {
            ESP_LOGE("Automower","Mode non géré : %s", value.c_str());
        }
    }

    void setup() override {
        
    }
    void sendCommands(int index) {
        if (index <= pollingCommandList.size()-1) {
            set_retry(
                5, // Temps d'attente initial en millisecondes
                3,    // Nombre maximal de tentatives
                [this,index](uint8_t attempt) -> RetryResult {
                    if (!_writable) {
                        return RetryResult::RETRY;
                    } else {
                        auto it = pollingCommandList.begin();
                        std::advance(it, index); // Move the iterator 'index' positions forward
                        this->write_array(*it,5);
                        this->_writable = false;
                        this->sendCommands(index+1);
                        return RetryResult::DONE;
                    }
                },
                2.0f  // Facteur d'augmentation du temps d'attente entre les tentatives
            );
        }
    }
    void update(){

        sendCommands(0);
        
    }



    void checkUartRead(){
        while(available() > 0 && peek() != 0x0F){
            read();
        }
        while (available() >= 5 && peek() == 0x0F) {

            uint8_t readData[5];
            read_array(readData,5);
            _writable = true;

            //char s[16];
            //sprintf(s,"%02x %02x %02x %02x %02x",readData[0],readData[1],readData[2],readData[3],readData[4]);
            //std::string s2(s);
            //lastCodeReceivedTextSensor->publish_state(s2);

            uint16_t receivedAddress = ((readData[1] & 0x7f) << 8) | readData[2];
            uint16_t receivedValue = (readData[4] << 8) | readData[3];
            
            switch(receivedAddress){
                case 0x012C:
                    publishMode(receivedValue);
                    break;
                case 0x01F1:
                    publishStatus(receivedValue);
                    break;
                case 0x01EF:
                    batteryLevelSensor->publish_state((float)receivedValue);
                    break;
                case 0x01EC:
                    chargingTimeSensor->publish_state((float)receivedValue);
                    break;
                case 0x2EE0:
                    batteryUsedSensor->publish_state((float)receivedValue);
                    break;
                case 0x2EF4:
                    batteryVoltageSensor->publish_state(((float)receivedValue)/1000.0f);
                    break;
                case 0x3390:
                    firmwareVersionSensor->publish_state((float)receivedValue);
                    break;

            }

        }
    }


    void publishMode(uint16_t receivedValue){
        std::string mode;
        switch(receivedValue){
            case 0x0000: 
                mode = "MAN";
                break;
            case 0x0001: 
                mode = "AUTO";
                break;
            case 0x0003: 
                mode = "HOME";
                break;
            case 0x0004: 
                mode = "DEMO";
                break;
            default: 
                char s[16];
                sprintf(s,"MODE_%04x",receivedValue);
                mode.assign(s);
                break;
        }
        modeTextSensor->publish_state(mode);
        
    }
    void publishStatus(uint16_t receivedValue){
        switch(receivedValue){

            case 0x0006: 
                statusTextSensor->publish_state("Moteur gauche bloqué");
                break;

            case 0x000C: 
                statusTextSensor->publish_state("Pas de signal de boucle");
                break;
            case 0x0010: 
                statusTextSensor->publish_state("Dehors");
                break;
            case 0x0012: 
                statusTextSensor->publish_state("Tension de batterie faible");
                break;
            case 0x001A: 
                statusTextSensor->publish_state("Borne de recharge bloquée");
                break;
            case 0x0022: 
                statusTextSensor->publish_state("Tondeuse levée");
                break;
            case 0x0034:
                statusTextSensor->publish_state("Pas de contact avec la station de charge");
                break;
            case 0x0036:
                statusTextSensor->publish_state("PIN expiré");
                break;
            case 0x03E8:
                statusTextSensor->publish_state("Sortie station");
                break;
            case 0x03EA:
                statusTextSensor->publish_state("Tonte en cours");
                break;
            case 0x03EE:
                statusTextSensor->publish_state("Démarrage de la tondeuse");
                break;
            case 0x03F0:
                statusTextSensor->publish_state("Tondeuse démarrée");
                break;
            case 0x03F4:
                statusTextSensor->publish_state("Signal de démarrage de la tondeuse");
                break;
            case 0x03F6:
                statusTextSensor->publish_state("En charge");
                break;
            case 0x03F8:
                statusTextSensor->publish_state("Waiting in station");
                break;
            case 0x0400:
                statusTextSensor->publish_state("Sortie station 2");
                break;
            case 0x040C:
                statusTextSensor->publish_state("Mode carrés");
                break;
            case 0x040E:
                statusTextSensor->publish_state("Coincé");
                break;
            case 0x0410:
                statusTextSensor->publish_state("Collision");
                break;
            case 0x0412:
                statusTextSensor->publish_state("Recherche");
                break;
            case 0x0414:
                statusTextSensor->publish_state("Arrêt");
                break;
            case 0x0418:
                statusTextSensor->publish_state("Amarrage");
                break;
            case 0x041A:
                statusTextSensor->publish_state("Sortie du mode de verrouillage de sécurité");
                break;
            case 0x041C:
                statusTextSensor->publish_state("Erreur");
                break;
            case 0x0420:
                statusTextSensor->publish_state("Waiting HOME");
                break;
            case 0x0422:
                statusTextSensor->publish_state("Suivre la limite");
                break;
            case 0x0424:
                statusTextSensor->publish_state("Signal trouvé");
                break;
            case 0x0426:
                statusTextSensor->publish_state("Coincé2");
                break;
            case 0x0428:
                statusTextSensor->publish_state("Recherche");
                break;
            case 0x042E:
                statusTextSensor->publish_state("Suivre la boucle de recherche");
                break;
            case 0x0430:
                statusTextSensor->publish_state("Suivre la boucle");
                break;


            default: 
                char s[16];
                sprintf(s,"STATUS_%04x",receivedValue);
                statusTextSensor->publish_state(s);
                break;
        }
        
    }

    void loop() override {

        checkUartRead();

    }

};
