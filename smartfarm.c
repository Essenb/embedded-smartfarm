//2019.05.21
//라즈베리 파이와 C언어를 사용하여 구현한 스마트 농장

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <wiringPi.h>
#include <mysql/mysql.h>

#define PUMP  21
#define FAN  22
#define DCMOTOR  23
#define RGBLEDPOWER 24
#define RED  7
#define GREEN  9
#define BLUE  8
#define LIGHTSEN_OUT 2
#define MAXTIMINGS 85

//MYSQL 연결 정보 정의
#define DBHOST "localhost" //IP 주소
#define DBUSER "root" //MYSQL 유저
#define DBPASS "root" //MYSQL 유저 비밀번호
#define DBNAME "iotfarm" //접속할 디비 이름

MYSQL *connector; //MYSQL 접속 포인터
MYSQL_RES *result; //결과 포인터
MYSQL_ROW row; //튜플을 저장할 변수

void Bpluspinmodeset(void); //센서들의 모드를 설정하는 함수
int wiringPicheck(void); //wiringPi가 제대로 정의되었는지 확인하는 함수
int get_light(); //빛 밝기 값을 받아오는 함수
int get_temperature_sensor(); //온도 값을 받아오는 함수
static uint8_t sizecvt(const int read); //온도, 습도 변수를 반환
int read_dht22_dat_temp(); //온도 값을 읽는 함수
int get_humidity_sensor(); //습도 값을 받아오는 함수
int read_dht22_dat_humid(); //습도 값을 읽는 함수
void act_waterpump_on(); //펌프 on 함수
void act_waterpump_off(); //펌프 off 함수
void act_fan_on(); //팬 on 함수
void act_fan_off(); //팬 off 함수
void act_dcmotor_on(); //모터 on 함수
void act_dcmotor_off(); //모터 off 함수
void act_rgbled_on(); //LED on 함수
void act_rgbled_off(); //LED off 함수

int ret_humid, ret_temp;
static int DHTPIN = 11;
static int dht22_dat[5] = {0, 0, 0, 0, 0};

int main(void)
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 Bpluspinmodeset();

 int curtemp = 0, curhumid = 0;

 //DB 연결
 connector = mysql_init(NULL);
 if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
	{
		fprintf(stderr, "%s\n", mysql_error(connector));
		return 0;
	}
 printf("MYSQL(rpidb) opened.\n");

 while(1)
 {
  char query[1024];
//get_light()가 0이면 led off. 1이면 led on
  if(get_light() == 0)
  {
   act_rgbled_off();
   printf("light!\n");
  }
  else
  {
   act_rgbled_on();
   printf("dark!\n");
  }
  curtemp = get_temperature_sensor();
  printf("%d \n", curtemp);
  printf("::check temp::\n");
//curtemp(온도)가 26도 이상이면 펌프 on. 2초 후에 off
  if(curtemp > 26)
  {
   printf("PUMP On\n");
   digitalWrite(PUMP, 1);
   act_waterpump_on();
   delay(2000);
   act_waterpump_off();
  }
  curhumid = get_humidity_sensor();
  printf("::check humid::\n");
//curhumid(습도)가 50 미만이면 팬 on. 2초 후에 off
  if(curhumid < 50)
  {
   printf("FAN On\n");
   act_fan_on();
   delay(2000);
   act_fan_off();
  }

//DB에 데이터 삽입
sprintf(query, "insert into envdata values (now(),%d,%d,%d)", get_light(), curtemp, curhumid);
if(mysql_query(connector, query))
{
   fprintf(stderr, "%s\n", mysql_error(connector));
   printf("Write DB error\n");
}
  delay(10000);
 }
 return 0;
}

int wiringPicheck(void)
{
 if(wiringPiSetup() == -1)
 {
  fprintf(stdout, "Unable to start wiringPi: %s \n", strerror(errno));
  return 1;
 }
}

void Bpluspinmodeset(void)
{
 pinMode(LIGHTSEN_OUT, INPUT);
 pinMode(PUMP, OUTPUT);
 pinMode(FAN, OUTPUT);
 pinMode(DCMOTOR, OUTPUT);
 pinMode(RGBLEDPOWER, OUTPUT);
 pinMode(RED, OUTPUT);
 pinMode(GREEN, OUTPUT);
 pinMode(BLUE, OUTPUT);
}

int get_light()
{
 if(wiringPiSetup() < 0)
 {
  fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
  return 1;
 }
 if(digitalRead(LIGHTSEN_OUT))
 {
  return 1;
 }
 else
 {
  return 0;
 }
}

int get_temperature_sensor()
{
 int received_temp;
 DHTPIN = 11;
 if(wiringPiSetup() == -1)
 {
  exit(EXIT_FAILURE);
 }
 if(setuid(getuid()) < 0)
 {
  perror("Dropping privileges failed\n");
  exit(EXIT_FAILURE);
 }
 while(read_dht22_dat_temp() == 0)
 {
  delay(500);
 }

 received_temp = ret_temp;
 printf("::Temperature = %d::\n", received_temp);
 return received_temp;
}

static uint8_t sizecvt(const int read)
{
 if(read > 255 || read < 0)
 {
  printf("Invalid data from wiringPi library\n");
  exit(EXIT_FAILURE);
 }
 return (uint8_t)read;
}

int read_dht22_dat_temp()
{
 uint8_t laststate = HIGH;
 uint8_t counter = 0;
 uint8_t j = 0, i;
 dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

 pinMode(DHTPIN, OUTPUT);
 digitalWrite(DHTPIN, HIGH);
 delay(10);

 digitalWrite(DHTPIN, LOW);
 delay(18);

 digitalWrite(DHTPIN, HIGH);
 delayMicroseconds(40);
 pinMode(DHTPIN, INPUT);

 for(i=0;i<MAXTIMINGS;i++)
 {
  counter = 0;
  while(sizecvt(digitalRead(DHTPIN)) == laststate)
  {
   counter++;
   delayMicroseconds(1);
   if(counter == 255)
   {
    break;
   }
  }
  laststate = sizecvt(digitalRead(DHTPIN));
  if(counter == 255) break;
  if((i>=4) && (i%2 == 0))
  {
   dht22_dat[j/8] <<= 1;
   if(counter > 50)
   {
    dht22_dat[j/8] |= 1;
   }
   j++;
  }
 }

 if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3] & 0xFF))))
 {
  float t, h;
  h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
  h /= 10;
  t = (float)(dht22_dat[2] & 0x7F) * 256 + (float)dht22_dat[3];
  t /= 10.0;

  if((dht22_dat[2] & 0x80) != 0)
  {
   t *= -1;
  }

  ret_humid = (int)h;
  ret_temp = (int)t;
  printf("Humidity = %.2f %%  Temperature = %.2f *C \n", h, t);
  printf("Humidity = %d Tempertaure = %d \n", ret_humid, ret_temp);

  return ret_temp;
 }
 else
 {
  printf("Data not 9ood, skip\n");
  return 0;
 }
}
int get_humidity_sensor()
{
 int received_humid;
 DHTPIN = 11;

 if(wiringPiSetup() == -1)
 {
  exit(EXIT_FAILURE);
 }

 if(setuid(getuid()) < 0)
 {
  perror("Dropping privileges failed\n");
  exit(EXIT_FAILURE);
 }

 while(read_dht22_dat_humid() == 0)
 {
  delay(500);
 }
 received_humid = ret_humid;
 printf("Humidity = %d\n", received_humid);

 return received_humid;
}

int read_dht22_dat_humid()
{
 uint8_t laststate = HIGH;
 uint8_t counter = 0;
 uint8_t j = 0, i;
 dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

 pinMode(DHTPIN, OUTPUT);
 digitalWrite(DHTPIN, HIGH);
 delay(10);

 digitalWrite(DHTPIN, LOW);
 delay(18);

 digitalWrite(DHTPIN, HIGH);
 delayMicroseconds(40);

 pinMode(DHTPIN, INPUT);
 for(i=0;i<MAXTIMINGS;i++)
 {
  counter = 0;
  while(sizecvt(digitalRead(DHTPIN)) == laststate)
  {
   counter++;
   delayMicroseconds(1);

   if(counter == 255)
   {
    break;
   }
  }
  laststate = sizecvt(digitalRead(DHTPIN));

  if(counter == 255) break;
  if((i>=4) && (i%2 == 0))
  {
   dht22_dat[j/8] <<= 1;
   if(counter > 50)
   {
    dht22_dat[j/8] |= 1;
   }
   j++;
  }
 }

 if((j >= 40) && (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3] & 0xFF))))
 {
  float t, h;
  h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
  h /= 10;
  t = (float)(dht22_dat[2] & 0x7F) * 256 + (float)dht22_dat[3];
  t /= 10.0;

  if((dht22_dat[2] & 0x80) != 0)
  {
   t *= -1;
  }
  ret_humid = (int)h;
  ret_temp = (int)t;
  printf("Humidity = %.2f %%  Temperature = %.2f *C \n", h, t);
  printf("Humidity = %d Tempertaure = %d \n", ret_humid, ret_temp);
  return ret_humid;
 }
 else
 {
  printf("Data not 9ood, skip\n");
  return 0;
 }
}

void act_waterpump_on()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(PUMP, OUTPUT);
 digitalWrite(PUMP, 1);
}

void act_waterpump_off()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(PUMP, OUTPUT);
 digitalWrite(PUMP, 0);
}

void act_fan_on()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(FAN, OUTPUT);
 digitalWrite(FAN, 1);
}

void act_fan_off()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(FAN, OUTPUT);
 digitalWrite(FAN, 0);
}

void act_dcmotor_on()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(DCMOTOR, OUTPUT);
 digitalWrite(DCMOTOR, 1);
}

void act_dcmotor_off()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(DCMOTOR, OUTPUT);
 digitalWrite(DCMOTOR, 0);
}

void act_rgbled_on()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(RGBLEDPOWER, OUTPUT);
 pinMode(RED, OUTPUT);
 pinMode(GREEN, OUTPUT);
 pinMode(BLUE, OUTPUT);

 digitalWrite(RGBLEDPOWER, 1);
 digitalWrite(BLUE, 1);
}

void act_rgbled_off()
{
 if(wiringPicheck())
 {
  printf("FAIL");
 }
 pinMode(RGBLEDPOWER, OUTPUT);
 pinMode(RED, OUTPUT);
 pinMode(GREEN, OUTPUT);
 pinMode(BLUE, OUTPUT);
 
 digitalWrite(RGBLEDPOWER, 0);
 digitalWrite(BLUE, 0);
}