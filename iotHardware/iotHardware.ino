#include <ESP8266WiFi.h>
/* 依赖 PubSubClient 2.4.0 */
#include <PubSubClient.h>
/* 依赖 ArduinoJson 5.13.4 */
#include <ArduinoJson.h>
#include <SimpleDHT.h>
#define Trig 5 //引脚Tring 连接 IO D1
#define Echo 4 //引脚Echo 连接 IO D2
#define pinDHT11 5 //温湿度传感器引脚
#define sensorPin 4//声音传感器引脚
//所连接的WiFi账密
#define WIFI_SSID         "ydjslzzdrr"
#define WIFI_PASSWD       "11111111"

//此处为服务器内部密钥验证配置，出于安全性考虑该处已省略
//此处为服务器内部密钥验证配置，出于安全性考虑该处已省略

float cm; //距离变量
float emp;//测量空垃圾桶的高度
float per;//垃圾占比
unsigned long lastMs = 0;
byte temperature1 = 0;
byte humidity1 = 0;
double capacity;
double temperature2;
double humidity2;
double soundIntensity;
double CO2;
int state;

float sensorValue = 0, filteredSignal = 0;



SimpleDHT11 dht11(pinDHT11);
WiFiClient espClient;
PubSubClient client(espClient);

//芯片联网初始化过程
void callback(char *topic, byte *payload, unsigned int length)
{
Serial.print("Message arrived [");
Serial.print(topic);
Serial.print("] ");
payload[length] = '\0';
Serial.println((char *)payload);
}
void wifiInit()
{
WiFi.mode(WIFI_STA);
WiFi.begin(WIFI_SSID, WIFI_PASSWD); //连接WiFi
while (WiFi.status() != WL_CONNECTED)
{
delay(1000);
Serial.println("WiFi not Connect");
}
Serial.println("Connected to AP");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());
Serial.print("espClient [");
client.setServer(MQTT_SERVER, MQTT_PORT); /* 连接WiFi之后，连接MQTT服务
器 */
client.setCallback(callback);
}

//MQTT连接到阿里云服务器
void mqttCheckConnect()
{
while (!client.connected())
{
Serial.println("Connecting to MQTT Server ...");
if (client.connect(CLIENT_ID, MQTT_USRNAME, MQTT_PASSWD))
{
Serial.println("MQTT Connected!");
}
else
{
Serial.print("MQTT Connect err:");
Serial.println(client.state());
delay(5000);
}
}
}


//将测量的各项数据上传到一级服务器
void mqttIntervalPost(double capacity,double temperature2,double humidity2,double soundIntensity,double CO2,int state)
{
char param[32];
char jsonBuf[128];
// Serial.print(capacity);
sprintf(param, "{\"capacity\":%lf,\"temperature\":%lf,\"humidity\":%lf,\"soundIntensity\":%lf,\"CO2\":%lf,\"state\":%d}",
capacity,temperature2,humidity2,soundIntensity,CO2,state);
// sprintf(param, "\"percentage\":%f}",per);
sprintf(jsonBuf, ALINK_BODY_FORMAT, param);
Serial.println(jsonBuf);
boolean d = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
if(d){
Serial.println("publish success");
}else{
Serial.println("publish fail");
}
}


//超声波测距仪，测量容器内容物高度
int heightMeasure(){
//给Trig发送一个低高低的短时间脉冲,触发测距
digitalWrite(Trig, LOW); //给Trig发送一个低电平
delayMicroseconds(2); //等待 2微妙
digitalWrite(Trig,HIGH); //给Trig发送一个高电平
delayMicroseconds(10); //等待 10微妙
digitalWrite(Trig, LOW); //给Trig发送一个低电平
float waitTime=float(pulseIn(Echo, HIGH));//存储回波等待时间,
//pulseIn函数会等待引脚变为HIGH,开始计算时间,再等待变为LOW并停止计时
//返回脉冲的长度
//声速是:340m/1s 换算成 34000cm / 1000000μs => 34 / 1000
//因为发送到接收,实际是相同距离走了2回,所以要除以2
//距离(厘米) = (回波时间 * (34 / 1000)) / 2
//简化后的计算公式为 (回波时间 * 17)/ 1000
float result=(waitTime * 17 )/1000; //把回波时间换算成cm
return result;
}


//温湿度传感器，测量空间温湿度
void temperatureMeasure(){
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature1, &humidity1, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return;
  }
  temperature2 = (double)temperature1;
  humidity2 = (double)humidity1;
 
}

//声音传感器，测量空间声强
void MainFunction() {
  sensorValue = (float) analogRead(sensorPin) * (5.0 / 1024.0);
  FilterSignal(sensorValue);
  Serial.print(sensorValue);
  Serial.print(" ");
  Serial.println(filteredSignal);
  soundIntensity=(double)filteredSignal;
}
void FilterSignal(float sensorSignal) {
  filteredSignal = (0.945 * filteredSignal) + (0.0549 * sensorSignal);
}



void setup()
{
/* initialize serial for debugging */
Serial.begin(115200);
Serial.println("Demo Start");
wifiInit();
unsigned char i=0;
pinMode(Trig, OUTPUT);
pinMode(Echo, INPUT);
emp=heightMeasure();
if(emp<5){
  emp=50;
  Serial.println("初始化失败，emp默认设置为50");
}
}



void loop()
{
delay(5000); //延时1000毫秒
cm = heightMeasure();
Serial.println(cm);
if(cm<=emp){
capacity=(emp-cm)/emp;
Serial.println(per);
}
if (millis() - lastMs >= 3000)
{
lastMs = millis();
mqttCheckConnect();
state= 100;
mqttIntervalPost(capacity,temperature2,humidity2,soundIntensity,CO2,state);//信息上报

}
client.loop();
}
