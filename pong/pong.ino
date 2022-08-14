#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
  
class player_class {
  public:
    int height;
};

class ball_class {
  public:
    float pos_x;
    float pos_y;
    float vX = 1;
    float vY = 0;
};

typedef struct struct_message_host {
  float ball_pos_x;
  float ball_pos_y;
  float left_player_height;
  int left_player_score;
  int right_player_score;
} struct_message_host;

struct_message_host hostData;

typedef struct struct_message_join {
  float right_player_height;
} struct_message_join;

struct_message_join joinData;

esp_now_peer_info_t peerInfo;

String program_state = "hostpick";

// global classes
player_class left_player;
player_class right_player;
ball_class ball;

// define score variables
int right_player_score = 0;
int left_player_score = 0;

// player variables
int player_length = 6;
int player_width = 3;

// networking variables:
bool is_host = true;
uint8_t hostMACAddress[] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0x66};
uint8_t joinMACAddress[] = {0x8F, 0x56, 0x9A, 0x00, 0x3D, 0xA8};

// ball variables
float max_angle = 1.2;

// Button global definitions
#define BUTTON_PIN1 19
int currentState1;

#define BUTTON_PIN2 23
int currentState2;

void setup() {
   Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // setup buttons
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);

  // setup players and ball
  ball.pos_x = 64;
  ball.pos_y = 30;
  left_player.height = 30;
  right_player.height = 30;

  // Clear the buffer
  display.clearDisplay();
  display.display();
}

void movePlayers() {
 currentState1 = digitalRead(BUTTON_PIN1);
 currentState2 = digitalRead(BUTTON_PIN2);
 if (is_host) {
    if (currentState1 == LOW) {
      left_player.height += 1;
    }
    if (currentState2 == LOW) {
      left_player.height -= 1;
    }
 } else {
   if (currentState1 == LOW) {
      right_player.height += 1;
    }
   if (currentState2 == LOW) {
      right_player.height -= 1;
    }
 }
}


void ballLogic() {
  // check if ball collides with player
  if (round(ball.pos_x) == player_width 
   && left_player.height - player_length <= ball.pos_y 
   && ball.pos_y <= left_player.height + player_length) {
      int difference_ball_player = left_player.height - ball.pos_y;
      float normalised_difference_ball_player = (float(difference_ball_player) / float(player_length));
      float bounceAngle = normalised_difference_ball_player * max_angle;
      ball.vX = cos(bounceAngle);
      ball.vY = -sin(bounceAngle);
  } else if (round(ball.pos_x) == 127 - player_width 
   && right_player.height - player_length <= ball.pos_y 
   && ball.pos_y <= right_player.height + player_length) {
      int difference_ball_player = right_player.height - ball.pos_y;
      float normalised_difference_ball_player = (float(difference_ball_player) / float(player_length));
      float bounceAngle = normalised_difference_ball_player * max_angle;
      ball.vX = -cos(bounceAngle);
      ball.vY = -sin(bounceAngle);
  }

  //check if ball has passed a player and update score
  if (ball.pos_x < 0) {
    ball.pos_x = 64;
    ball.pos_y = 30;
    ball.vY = 0.0;
    ball.vX = -1.0;
    right_player_score += 1;
  } else if (ball.pos_x > SCREEN_WIDTH - 1) {
    ball.vY = 0.0;
    ball.vX = 1.0;
    ball.pos_x = 64;
    ball.pos_y = 30;
    left_player_score += 1;
  } else if (ball.pos_y > SCREEN_HEIGHT - 1 || ball.pos_y < 0) {
    ball.vY = ball.vY * - 1.0;
  }
  
  ball.pos_x += ball.vX;
  ball.pos_y += ball.vY;
  
}

void drawPixels() {
  display.clearDisplay();
  // plot left player:
    for (int width = 0; width <= player_width - 1; width++) {
      for (int player_size = 0; player_size <= player_length; player_size++) {
          display.drawPixel(width, left_player.height + player_size, SSD1306_WHITE);
          display.drawPixel(width, left_player.height - player_size, SSD1306_WHITE);
        }
    }
    
   // plot right player:
    for (int width = 0; width <= player_width - 1; width++) {
      for (int player_size = 0; player_size <= player_length; player_size++) {
          display.drawPixel(SCREEN_WIDTH - 1 - width, right_player.height + player_size, SSD1306_WHITE);
          display.drawPixel(SCREEN_WIDTH - 1 - width, right_player.height - player_size, SSD1306_WHITE);
        }
    }
    
  //plot ball
    display.drawPixel(round(ball.pos_x), round(ball.pos_y), SSD1306_WHITE);


  // plot score
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(SCREEN_WIDTH/2 - 10, 3);
    display.println(left_player_score);
    display.setCursor(SCREEN_WIDTH/2 + 10, 3);
    display.println(right_player_score);
    
  display.display();
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  return;
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (is_host) {
        memcpy(&joinData, incomingData, sizeof(joinData));
        right_player.height = joinData.right_player_height;
    } else {
        memcpy(&hostData, incomingData, sizeof(hostData));
        left_player.height = hostData.left_player_height;
        ball.pos_x = hostData.ball_pos_x;
        ball.pos_y = hostData.ball_pos_y;
        left_player_score = hostData.left_player_score;
        right_player_score = hostData.right_player_score;
    }
}

void hostSendPackage() {
  hostData.ball_pos_x = ball.pos_x;
  hostData.ball_pos_y = ball.pos_y;
  hostData.left_player_height = left_player.height;
  hostData.left_player_score = left_player_score;
  hostData.right_player_score = right_player_score;
  esp_now_send(joinMACAddress, (uint8_t *) &hostData, sizeof(hostData));
}

void joinSendPackage() {
  joinData.right_player_height = right_player.height;
  esp_now_send(hostMACAddress, (uint8_t *) &joinData, sizeof(joinData));
}

void loop() {
  if (program_state == "playing") {
        // read button states and move player
        movePlayers();
        // perform ball logic
        // send states
        if (is_host) {
            ballLogic();
            hostSendPackage();
        } else {
            joinSendPackage();
        }
        // draw pixels
        drawPixels();
  } else if (program_state == "hostpick") {
    // display options
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0 + 3, 3);
    display.println("Press up to be host or down to join");
    display.display();
    
    // check for choice and do a setup
    currentState1 = digitalRead(BUTTON_PIN1);
    currentState2 = digitalRead(BUTTON_PIN2);
    if (currentState1 == LOW || currentState2 == LOW) {
       WiFi.mode(WIFI_STA);
       peerInfo.channel = 0;  
       peerInfo.encrypt = false;
       
       if (currentState1 == LOW) {
        is_host = false;
        // change mac address to a specific one for joining
        esp_wifi_set_mac(WIFI_IF_STA, &joinMACAddress[0]);
        // Register peer
        memcpy(peerInfo.peer_addr, hostMACAddress, 6);
      } else if (currentState2 == LOW) {
        is_host = true;
        // change mac address to a specific one for hosting
        esp_wifi_set_mac(WIFI_IF_STA, &hostMACAddress[0]);
        // Register peer
        memcpy(peerInfo.peer_addr, joinMACAddress, 6);
      }
      if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
      }
      
      // Add peer        
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
      }
      
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      
      program_state = "playing";
    }
  }
}
