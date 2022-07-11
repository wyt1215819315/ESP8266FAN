#define BLINKER_WIFI
#define BLINKER_MIOT_FAN
#define BLINKER_ALIGENIE_FAN
//------------------------输出脚的定义---------------
#define PW_LOW D5         // 继电器 低
#define PW_MIDDLE D6         // 继电器 中
#define PW_HIGH D7         // 继电器 高
#define PW_HEAD D8         // 继电器 摇头
#define LED_POWER LED_BUILTIN        // LED 主
#define LED_HEAD  D3        // LED 摇头
#define D_BIBI D9         // 蜂鸣器
//-------------------LED闪烁状态定义--------------
#define LED_STATUS_LOW 1500         // LED 低速
#define LED_STATUS_MIDDLE 700         // LED 中速
#define LED_STATUS_HIGH 200         // LED 高速
//-------------------------------- 关于按键的一些定义-----------------------------
#define PLUS D1         // D1开关
#define SUB D2         // D2开关
 
//读按键值
#define KEY1      digitalRead(PLUS)
#define KEYS      digitalRead(SUB)
#define PIN_HEAD      digitalRead(PW_HEAD)
#define PIN_LED_POWER      digitalRead(LED_POWER)
#define PIN_LED_HEAD      digitalRead(LED_HEAD)
#define PIN_BIBI      digitalRead(D_BIBI)
 
#define short_press_time 10     //短按时间
#define long_press_time 1000    //长按时间
#define sound_bibi_time_short 100    //蜂鸣器短时间
#define sound_bibi_time_long 800    //蜂鸣器长时间

 //没事不要动这个
#define NO_KEY_PRES       0   //无按键按下
#define PLUS_KEY_PRES     1   //KEY1短按定义
#define SUB_KEY_PRES      2
#define PLUS_KEY_LONG_PRES     11 //KEY1长按定义
#define SUB_KEY_LONG_PRES      22

#include <Blinker.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>

char auth[] = "2a8369da9a45";
char ssid[] = "oldwuHome";
char pswd[] = "asdfghjkl805.";

Ticker ledTickLow;                    // LED闪烁定时器
Ticker ledTickMiddle;                    // LED闪烁定时器
Ticker ledTickHigh;                    // LED闪烁定时器
Ticker keyTick;                    // 按键检测定时器
Ticker bbTickShort;                    // 蜂鸣器异步定时器
Ticker bbTickLong;                    // 蜂鸣器异步定时器

// 新建组件对象
BlinkerButton Button1("btn-fan-power");
BlinkerButton Button2("btn-fan-speed");
BlinkerButton Button3("btn-fan-head-power");
BlinkerButton Button4("btn-ota");
BlinkerSlider Slider1("ran-fan-speed");

uint32_t os_time = 0;
int ledMainFlag = 0;   //主led状态，0关闭，1常亮，2low闪，3middle闪，4快闪
int ledHeadFlag = 0;   //副led状态，0关闭，1常亮，2low闪，3middle闪，4快闪
bool KEYP_UP = 0;                  //按键状态，0为弹起，1为按下
bool KEYS_UP = 0;                  //按键状态，0为弹起，1为按下
uint16_t KEYP_PRESS_COUNT = 0;     //按KEYU时间计数
uint16_t KEYS_PRESS_COUNT = 0;     //按KEYM时间计数
uint8_t KEYP_KEY_READ = NO_KEY_PRES;//KEYU按键状态
uint8_t KEYS_KEY_READ = NO_KEY_PRES;//KEYM按键状态
//-----------自定义状态变量------------
bool FAN_POWER = 0;                     //电源状态
bool FAN_HEAD = 0;                     //风扇摇头状态
bool FAN_STATUS_SPEED_CHANGE = 1;  //判断状态是否变动，是否需要调整
int FAN_SPEED = 1;      //风速 1低2中3高
bool bb_flag_long = 0;
bool bb_flag_short = 0;
int bb_type = 0;
int bb_num = 1;
String updateUrl = "http://192.168.123.165:80/fan.ino.nodemcu.bin";


//OTA升级
void update() {
  WiFiClient client;
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);
  ESPhttpUpdate.update(client,updateUrl);
}
void update_error(int err) {
  Serial.print(String(" HTTP 升级固件错误或失败！") + String(err));
}

void update_progress(int cur, int totaol) {
  if (millis() - os_time >= 1000) {
      os_time = millis();
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  Serial.println(String(String(String(String("正在更新文件") + String(cur)) + String("bit")) + String(totaol)) + String("bytes"));
}

void update_finished() {
  Serial.println("恭喜你，更新固件成功！系统将会重新启动！");
}

void update_started() {
  Button4.text("开始更新固件");
  Serial.println("开始更新固件");
}

void button4_callback(const String & state) {
  update();
}


//----------智能家居支持--------------
//----------小爱同学
//开关电源
void xiaomiPowerState(const String & state){
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON) {
        FAN_POWER = 1;
        BlinkerMIOT.powerState("on");
        BlinkerMIOT.print();
    }
    else if (state == BLINKER_CMD_OFF) {
        FAN_POWER = 0;
        BlinkerMIOT.powerState("off");
        BlinkerMIOT.print();
    }
    FAN_STATUS_SPEED_CHANGE = 1;
    initFan();
}
void xiaomiMiotLevel(uint8_t level){
    BLINKER_LOG("need set level: ", level);
    // 0:AUTO MODE, 1-3 LEVEL
    if(level == 0){
      level = 1;
    }
    FAN_SPEED = level;
    FAN_STATUS_SPEED_CHANGE = 1;
    BlinkerMIOT.level(level);
    BlinkerMIOT.print();
    initFan();
}
void miotHSwingState(const String & state){
    BLINKER_LOG("need set HSwing state: ", state);
    // horizontal-swing
    if (state == BLINKER_CMD_ON) {
        FAN_HEAD = 1;
        BlinkerMIOT.hswing("on");
        BlinkerMIOT.print();
    }else if (state == BLINKER_CMD_OFF) {
        FAN_HEAD = 0;
        BlinkerMIOT.hswing("off");
        BlinkerMIOT.print();
    }
    initFan();
}
void miotVSwingState(const String & state){
    BLINKER_LOG("need set HSwing state: ", state);
    // horizontal-swing
    if (state == BLINKER_CMD_ON) {
        FAN_HEAD = 1;
        BlinkerMIOT.vswing("on");
        BlinkerMIOT.print();
    }else if (state == BLINKER_CMD_OFF) {
        FAN_HEAD = 0;
        BlinkerMIOT.vswing("off");
        BlinkerMIOT.print();
    }
    initFan();
}
void miotQuery(int32_t queryCode){
    BLINKER_LOG("MIOT Query codes: ", queryCode);
    switch (queryCode){
        case BLINKER_CMD_QUERY_ALL_NUMBER :
            BLINKER_LOG("MIOT Query All");
            BlinkerMIOT.hswing(FAN_HEAD ? "on" : "off");
            BlinkerMIOT.vswing(FAN_HEAD ? "on" : "off");
            BlinkerMIOT.powerState(FAN_POWER ? "on" : "off");
            BlinkerMIOT.level(FAN_SPEED);
            BlinkerMIOT.print();
            break;
        case BLINKER_CMD_QUERY_POWERSTATE_NUMBER :
            BLINKER_LOG("MIOT Query Power State");
            BlinkerMIOT.powerState(FAN_POWER ? "on" : "off");
            BlinkerMIOT.print();
            break;
        default :
            BlinkerMIOT.powerState(FAN_POWER ? "on" : "off");
            BlinkerMIOT.hswing(FAN_HEAD ? "on" : "off");
            BlinkerMIOT.vswing(FAN_HEAD ? "on" : "off");
            BlinkerMIOT.level(FAN_SPEED);
            BlinkerMIOT.print();
            break;
    }
}

//-------------天猫精灵
//用户设置风速/挡位控制的回调函数: 风扇/空调:
void aligenieLevel(uint8_t level){
    BLINKER_LOG("need set level: ", level);
    // 0:AUTO MODE, 1-3 LEVEL
    if(level == 0){
      level = 1;
    }
    FAN_SPEED = level;
    FAN_STATUS_SPEED_CHANGE = 1;
    BlinkerAliGenie.level(level);
    BlinkerAliGenie.print();
    initFan();
}
//用户步长设置风速/挡位控制的回调函数: 风扇:
void aligenieRelativeLevel(int32_t level){
    BLINKER_LOG("need set relative level: ", level);
    BlinkerAliGenie.level(FAN_SPEED + 1);
    BlinkerAliGenie.print();
    addSpeed();
}
//用户设置风扇/空调左右摆风的回调函数:
void aligenieHSwingState(const String & state){
    BLINKER_LOG("need set HSwing state: ", state);
    // horizontal-swing

    if (state == BLINKER_CMD_ON) {
        FAN_HEAD = 1;
        BlinkerAliGenie.hswing("on");
        BlinkerAliGenie.print();
    }else if (state == BLINKER_CMD_OFF) {
        FAN_HEAD = 0;
        BlinkerAliGenie.hswing("off");
        BlinkerAliGenie.print();
    }
    initFan();
}
void aligenieVSwingState(const String & state){
    BLINKER_LOG("need set HSwing state: ", state);
    // horizontal-swing

    if (state == BLINKER_CMD_ON) {
        FAN_HEAD = 1;
        BlinkerAliGenie.vswing("on");
        BlinkerAliGenie.print();
    }else if (state == BLINKER_CMD_OFF) {
        FAN_HEAD = 0;
        BlinkerAliGenie.vswing("off");
        BlinkerAliGenie.print();
    }
    initFan();
}
void aligeniePowerState(const String & state){
    BLINKER_LOG("need set power state: ", state);

    if (state == BLINKER_CMD_ON) {
        FAN_POWER = 1;
        BlinkerAliGenie.powerState("on");
        BlinkerAliGenie.print();
    }
    else if (state == BLINKER_CMD_OFF) {
        FAN_POWER = 0;
        BlinkerAliGenie.powerState("off");
        BlinkerAliGenie.print();
    }
    FAN_STATUS_SPEED_CHANGE = 1;
    initFan();
}
void aligenieQuery(int32_t queryCode){
    BLINKER_LOG("AliGenie Query codes: ", queryCode);

    switch (queryCode){
        case BLINKER_CMD_QUERY_ALL_NUMBER :
            BLINKER_LOG("AliGenie Query All");
            BlinkerAliGenie.hswing(FAN_HEAD ? "on" : "off");
            BlinkerAliGenie.vswing(FAN_HEAD ? "on" : "off");
            BlinkerAliGenie.powerState(FAN_POWER ? "on" : "off");
            BlinkerAliGenie.level(FAN_SPEED);
            BlinkerAliGenie.print();
            break;
        case BLINKER_CMD_QUERY_POWERSTATE_NUMBER :
            BLINKER_LOG("AliGenie Query Power State");
            BlinkerAliGenie.powerState(FAN_POWER ? "on" : "off");
            BlinkerAliGenie.print();
            break;
        default :
            BlinkerAliGenie.hswing(FAN_HEAD ? "on" : "off");
            BlinkerAliGenie.vswing(FAN_HEAD ? "on" : "off");
            BlinkerAliGenie.powerState(FAN_POWER ? "on" : "off");
            BlinkerAliGenie.level(FAN_SPEED);
            BlinkerAliGenie.print();
            break;
    }
}



void init_blinker_power_button(){
  if(FAN_POWER == 0){
    Button1.color("#FF0000");
    Button1.text("打开风扇");
    Button1.print("off");
  }else{
    Button1.color("#4169E1");
    Button1.text("关闭风扇");
    Button1.print("on");
  }
}

void init_blinker_speed(){
  Button2.icon("fan");
  Slider1.print(FAN_SPEED);
}

void init_blinker_head_power(){
  if(FAN_HEAD == 0){
    Button3.color("#FF0000");
    Button3.text("打开摇头");
    Button3.print("off");
  }else{
    Button3.color("#4169E1");
    Button3.text("关闭摇头");
    Button3.print("on");
  }
}

// 按下按键即会执行该函数
void button1_callback(const String & state) {
  turnPower();
  init_blinker_power_button();
}

void button2_callback(const String & state) {
  addSpeed();
  init_blinker_speed();
  init_blinker_power_button();
}

void button3_callback(const String & state) {
  turnHeadPower();
  init_blinker_head_power();
}

void slider1_callback(int32_t value){
  FAN_SPEED = value;
  FAN_STATUS_SPEED_CHANGE = 1;
  initFan();
  init_blinker_speed();
}

void addSpeed() {
  //增加速度
  //如果风扇没开启电源则响应为开启电源
  if(FAN_POWER == 0){
    turnPower();
    return;
  }
  //判断当前速度
  if(FAN_SPEED < 3){
    FAN_SPEED++;
  }else{
    FAN_SPEED = 1;
  }
  FAN_STATUS_SPEED_CHANGE = 1;
  initFan();
}

void turnPower() {
  //开关电源
  FAN_POWER = !FAN_POWER;
  FAN_STATUS_SPEED_CHANGE = 1;
  initFan();
}

void turnHeadPower() {
  //开关风扇摇头
  //如果风扇没开启电源则不做响应
  if(FAN_POWER == 0){
    return;
  }
  FAN_HEAD = !FAN_HEAD;
  initFan();
}

void initBibi(){
  bb_flag_long = 0;
  bb_flag_short = 0;
  bb_num = 1;
}

//type=1时为正常响，type=2时为长鸣
void bibi(int type,int num){
  if(PIN_BIBI){
    initBibi();
    digitalWrite(D_BIBI,LOW);
  }
  if(type == 2){
    bb_flag_long = 1;
  }else{
    bb_flag_short = 1;
  }
  bb_num = num;
}

void bbAsyncLong(){
  //蜂鸣器控制bi
  if(bb_flag_long){
    if(PIN_BIBI){
      digitalWrite(D_BIBI,LOW);
      bb_num = bb_num - 1;
      return;
    }
    if(bb_num <= 0){
      initBibi();
      return;
    }
    digitalWrite(D_BIBI,HIGH);
  }
}

void bbAsyncShort(){
  //蜂鸣器控制bi
  if(bb_flag_short){
    if(PIN_BIBI){
      digitalWrite(D_BIBI,LOW);
      bb_num = bb_num - 1;
      return;
    }
    if(bb_num <= 0){
      initBibi();
      return;
    }
    digitalWrite(D_BIBI,HIGH);
  }
}

//如果改变了状态都需要调用一下这个类
void initFan() {
  Serial.println("initFan...");
  Serial.print("Power=");
  Serial.println(FAN_POWER);
  Serial.print("Speed=");
  Serial.println(FAN_SPEED);
  Serial.print("Head=");
  Serial.println(FAN_HEAD);
  //读取保存在变量中的值来控制风扇
  if(FAN_POWER){
    //电源打开，判断风速
    if(FAN_STATUS_SPEED_CHANGE){
      if(FAN_SPEED==1){
        digitalWrite(PW_LOW,HIGH);
        digitalWrite(PW_MIDDLE,LOW);
        digitalWrite(PW_HIGH,LOW);
        bibi(1,1);
        ledMainFlag = 2;
      }else if(FAN_SPEED==2){
        digitalWrite(PW_LOW,LOW);
        digitalWrite(PW_MIDDLE,HIGH);
        digitalWrite(PW_HIGH,LOW);
        bibi(1,2);
        ledMainFlag = 3;
      }else if(FAN_SPEED==3){
        digitalWrite(PW_LOW,LOW);
        digitalWrite(PW_MIDDLE,LOW);
        digitalWrite(PW_HIGH,HIGH);
        bibi(1,3);
        ledMainFlag = 4;
      }
    }
    //判断摇头状态
    if(FAN_HEAD){
      if(!PIN_HEAD){
        digitalWrite(PW_HEAD,HIGH);
        bibi(1,1);
        ledHeadFlag = 1;
      }
    }else{
      if(PIN_HEAD){
        digitalWrite(PW_HEAD,LOW);
        bibi(1,1);
        ledHeadFlag = 0;
      }
    }
  }else{
    //电源关闭
    digitalWrite(PW_LOW,LOW);
    digitalWrite(PW_MIDDLE,LOW);
    digitalWrite(PW_HIGH,LOW);
    digitalWrite(PW_HEAD,LOW);
    bibi(2,1);
    ledMainFlag = 1;
    ledHeadFlag = 0;
  }
  FAN_STATUS_SPEED_CHANGE = 0;
  //写入rom来保存当前状态
  saveStatus();
}

void saveStatus(){
  Serial.println("保存状态...");
  EEPROM.begin(1400);
  int data1 = FAN_POWER%256;
  EEPROM.write(1300, data1);
  int data2 = FAN_SPEED%256;
  EEPROM.write(1301, data2);
  int data3 = FAN_HEAD%256;
  EEPROM.write(1302, data3);
  EEPROM.commit();
}


//按键服务
void flip() {
  //PLUS_KEY
  if(KEY1==0){
    KEYP_PRESS_COUNT++;
    KEYP_UP = 1;
    if(KEYP_PRESS_COUNT<=short_press_time)
      KEYP_KEY_READ = NO_KEY_PRES;
    if(KEYP_PRESS_COUNT>=short_press_time&&KEYP_PRESS_COUNT<=long_press_time)
      KEYP_KEY_READ = PLUS_KEY_PRES;
    if(KEYP_PRESS_COUNT>=long_press_time)
      KEYP_KEY_READ = PLUS_KEY_LONG_PRES;
  }
  if(KEY1){
    KEYP_PRESS_COUNT = 0;
    KEYP_UP = 0;//按键状态，0为弹起，1为按下
  }
  //SUB_KEY
  if(KEYS==0){
    KEYS_PRESS_COUNT++;
    KEYS_UP = 1;
    if(KEYS_PRESS_COUNT<=short_press_time)
      KEYS_KEY_READ = NO_KEY_PRES;
    if(KEYS_PRESS_COUNT>=short_press_time&&KEYS_PRESS_COUNT<=long_press_time)
      KEYS_KEY_READ = SUB_KEY_PRES;
    if(KEYS_PRESS_COUNT>=long_press_time)
      KEYS_KEY_READ = SUB_KEY_LONG_PRES;
  }
  if(KEYS){
    KEYS_PRESS_COUNT = 0;
    KEYS_UP = 0;//按键状态，0为弹起，1为按下
  }
  
  if(KEYP_KEY_READ == PLUS_KEY_LONG_PRES && KEYP_UP == 0){
    Serial.println("--KEY_PLUS长按");
    //重置KEY状态
    KEYP_KEY_READ = NO_KEY_PRES;
    //调整继电器状态，长按定义为开关机
    turnPower();
  }
  if(KEYP_KEY_READ == PLUS_KEY_PRES && KEYP_UP == 0){
    Serial.println("--KEY_PLUS短按");
    //重置KEY状态
    KEYP_KEY_READ = NO_KEY_PRES;
    //开启或者增加风速
    addSpeed();
  }
  if(KEYS_KEY_READ == SUB_KEY_LONG_PRES && KEYS_UP == 0){
    Serial.println("--KEY_SUB长按");
    KEYS_KEY_READ = NO_KEY_PRES;
    //暂时无定义的操作
    turnHeadPower();
  }
  if(KEYS_KEY_READ == SUB_KEY_PRES && KEYS_UP == 0){
    Serial.println("--KEY_SUB短按");
    KEYS_KEY_READ = NO_KEY_PRES;
    turnHeadPower();
  }
}

void checkLedAndTurn(int type){
  if(ledMainFlag == 0 && PIN_LED_POWER){
    digitalWrite(LED_POWER,LOW);
    return;
  }
  if(ledMainFlag == 1 && !PIN_LED_POWER){
    digitalWrite(LED_POWER,HIGH);
    return;
  }
  if(ledHeadFlag == 0 && PIN_LED_HEAD){
    digitalWrite(LED_HEAD,LOW);
    return;
  }
  if(ledHeadFlag == 1 && !PIN_LED_HEAD){
    digitalWrite(LED_HEAD,HIGH);
    return;
  }
  if(ledMainFlag == type){
    digitalWrite(LED_POWER,!PIN_LED_POWER);
  }
  if(ledHeadFlag == type){
    digitalWrite(LED_HEAD,!PIN_LED_HEAD);
  }
}

void ledBlinkLow(){
  checkLedAndTurn(2);
}

void ledBlinkMiddle(){
  checkLedAndTurn(3);
}

void ledBlinkHigh(){
  checkLedAndTurn(4);
}


//基础初始化
void initBasic(){
  delay(1000);
  Serial.begin(115200);   // 串口波特率
  EEPROM.begin(1400);     // 开启EEPROM，开辟1024个位空间
  //-----定义引脚状态------
  //这两个led脚和tx rx一样，调试的话需要关掉
  pinMode(PW_LOW, OUTPUT);
  pinMode(PW_MIDDLE, OUTPUT);
  pinMode(PW_HIGH, OUTPUT);
  pinMode(PW_HEAD, OUTPUT);
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_HEAD, OUTPUT);
  pinMode(D_BIBI, OUTPUT);
  // 初始化板载LED的IO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("初始化输出引脚模式完成...");
  
  delay(50);
  
  //prepare interupt  设置外置按钮管脚为上拉输入模式
  pinMode(PLUS,INPUT_PULLUP);
  pinMode(SUB,INPUT_PULLUP);
  Serial.println("初始化按钮引脚模式完成...");
 
  //adcTick.attach(5, adcService);   //电压检测定时器（每5秒检测一次）
  ledTickLow.attach_ms(LED_STATUS_LOW, ledBlinkLow);//led低频闪烁定时器
  ledTickMiddle.attach_ms(LED_STATUS_MIDDLE, ledBlinkMiddle);//led中频闪烁定时器
  ledTickHigh.attach_ms(LED_STATUS_HIGH, ledBlinkHigh);//led高频闪烁定时器
  keyTick.attach_ms(1, flip);      //按键检测定时器（每1毫秒检测一次）
  bbTickShort.attach_ms(sound_bibi_time_short, bbAsyncShort);      //蜂鸣器异步定时器，防止阻塞
  bbTickLong.attach_ms(sound_bibi_time_long, bbAsyncLong);      //蜂鸣器异步定时器，防止阻塞

 //读取rom中存的值
  int eeprom_str_power=EEPROM.read(1300);
  int eeprom_str_speed=EEPROM.read(1301);
  int eeprom_str_head=EEPROM.read(1302);
  Serial.print("eeprom_str_power=");
  Serial.println(eeprom_str_power);
  Serial.print("eeprom_str_speed=");
  Serial.println(eeprom_str_speed);
  Serial.print("eeprom_str_head=");
  Serial.println(eeprom_str_head);
  //FAN_POWER = eeprom_str_power;
  if(eeprom_str_speed != 255){
    FAN_SPEED = eeprom_str_speed;
  }
  FAN_HEAD = eeprom_str_head;
  //初始化风扇
  initFan();
}

void setup() {
   //首先初始化所有基础
    initBasic();
    //调用blinker初始化
    BLINKER_DEBUG.stream(Serial);
    
    // 初始化blinker
    Blinker.begin(auth, ssid, pswd);
    //注册blinker回调函数
    Button1.attach(button1_callback);
    Button2.attach(button2_callback);
    Button3.attach(button3_callback);
    Button4.attach(button4_callback);
    Slider1.attach(slider1_callback);
    //注册小爱同学回调函数
    BlinkerMIOT.attachPowerState(xiaomiPowerState);
    BlinkerMIOT.attachLevel(xiaomiMiotLevel);
    BlinkerMIOT.attachHSwing(miotHSwingState);
    BlinkerMIOT.attachVSwing(miotVSwingState);
    BlinkerMIOT.attachQuery(miotQuery);
    //注册天猫精灵回调函数
    BlinkerAliGenie.attachPowerState(aligeniePowerState);
    BlinkerAliGenie.attachLevel(aligenieLevel);
    BlinkerAliGenie.attachRelativeLevel(aligenieRelativeLevel);
    BlinkerAliGenie.attachHSwing(aligenieHSwingState);
    BlinkerAliGenie.attachVSwing(aligenieVSwingState);
    BlinkerAliGenie.attachQuery(aligenieQuery);
}



void loop() {
    Blinker.run();
}
