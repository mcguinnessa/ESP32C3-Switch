// define led according to pin diagram
int led1 = D10;
int led2 = D1;

void setup() {
  Serial.begin(115200);
  // initialize digital pin led as an output
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  while(!Serial);
  Serial.println("Starting...");
}

void loop() {

  Serial.println("LED on D10 on");
  digitalWrite(led1, HIGH);   // turn the LED on   
  delay(5000);               // wait for a second
  Serial.println("LED on D10 off");
  digitalWrite(led1, LOW);    // turn the LED off
  
  delay(1000);               // wait for a second

  Serial.println("LED on D1 on");
  digitalWrite(led2, HIGH);   // turn the LED on   
  delay(5000);               // wait for a second
  Serial.println("LED on D1 off");
  digitalWrite(led2, LOW);    // turn the LED off
  delay(1000);               // wait for a second

}
