//this library is just a clone of the digiMidi library, just wih the vid/pid and name of device changed
#include <USB_Stylocard.h>
void write_eeprom(uint16_t address,uint8_t data) {
  EEDR = data;
  EEAR = address;
  EECR |= (1 << EEMPE);
  EECR |= (1 << EEPE);
  while (EECR & (1 << EEPE));
  EECR = 0;
}
uint8_t read_eeprom(uint16_t address) {
  EEAR = address;
  EECR |= (1 << EERE);
  return EEDR;
}
USB_Stylocard midi;
int voltage[20] = {
  //do not pay attention to these setting, they will not work
  0,
  339,
  511,
  613,
  682,
  730,
  767,
  795,
  818,
  837,
  852,
  865,
  876,
  886,
  895,
  902,
  909,
  915,
  920,
  925
};
void setup() {
  //set PB0 and PB1 high while they are inputs (set the internal pull up resistors)
  PORTB |= (1 << PB0) | (1 << PB1);
  //check if there is data on the EEPROM
  if (read_eeprom(2)) {
    for (int a = 0; a != 20; a++) {
      //read the data that we have written in the EEPROM
      //since the adc value wont fit on just one EEPROM sector (because it can only store an 8 bit variable) we take the lower 8 bits of the first address
      //and the higher 2 bits and just shift the higher bits by 8 spaces (bits) and just add them up
      //example: 1011010110=726
      //10 high bits  11010110 low bits. then store each in a different EEPROM address
      //to reconstruct the number just do: (10*2^8) + 11010110
      voltage[a] = read_eeprom(a << 1) | (read_eeprom((a << 1) + 1) << 8);
    }
  }
  midi.delay(1500);
  TCCR1 = 0b10001011;
  OCR1A = 156;
  TIMSK |= (1 << OCIE1A);
  //Set reference value to VCC
  //Use ADC1 (PB2)
  ADMUX  = 0b00000001;
  //enable the ADC and start a convertion, set ADC prescaler to 4
  ADCSRA = 0b11000010;
}
//variables
uint16_t transpose = 45;
const uint8_t noise = 7;
byte note;
byte note_send = 255;
int prev_read;
byte count;
int adc_real_read;
long time;
long time2;
int getadc() {
  byte count2;
  int prev_read2;
  int adc_real_read2;
  while (1) {
    //start an ADC calculation and reset the ADC converion flag
    ADCSRA |= (1 << ADSC) | (1 << ADIF);
    //wait for the ADC convertion to end
    while (!(ADCSRA & (1 << ADIF)));
    int adc = ADC;
    //i will just resume what this part does.
    //it reads a 100 times the adc value and check if the value of all those reading are the same by a +-3 offset
    //if one of those values are not in that range it will redo all the calculations
    if ((adc < (prev_read2 + noise)) && (adc > (prev_read2 - noise))) {
      count2++;
    }
    else {
      count2 = 0;
      prev_read2 = adc;
    }
    //if we succeed, break from the while(1)
    if (count2 == 100) {
      count2 = 0;
      adc_real_read2 = adc;
      break;
    }
  }
  //return the adc reading value
  return adc_real_read2;
}
ISR(TIMER1_COMPA_vect) {
  // usb midi thing
  midi.update();
}
bool wait[2];
void loop() {
  //debouncing
  //if PB1 is low
  if (!(PINB & (1 << PB1))) {
    //and wait[0] is 0
    if (!wait[0]) {
      //write the current millis to the variable time
      time = millis();
      //also set the wait bool so that we dont repeat this
      wait[0] = 1;
    }
  }
  //if PB1 is high
  else {
    //and the wait bool has been set
    if (wait[0]) {
      //check if we have been holding the PB0 button in between 150-1000 milliseconds
      if (((millis() - time) > 60) && ((millis() - time) < 1000)) {
        //transpose down the notes by an octave
        if ((transpose - 12) >= 0) {
          transpose -= 12;
        }
      }
      //if we instead press it for longer than 1 sec
      if ((millis() - time) > 1000) {
        //note adc value calibration routine
        //we will calibrate all the 20 notes
        for (int a = 0; a != 20; a++) {
          //wait for the PB0 button to be press
          while (PINB & (1 << PB0));
          //get the adc value
          int value = getadc();
          //if this value is higher than 900 (meaning that we have not press any key) skip this
          if (value < 900) {
            //since we have not done another adc calculation the ADC register will have the same value
            //write the ADC register value in to the corresponding voltage variable array
            voltage[a] = ADC;
            //write the low ADC byte in the first EEPROM address
            write_eeprom(a << 1, ADCL);
            //write the higher ADC byte int the next address
            write_eeprom((a << 1) + 1, ADCH);
          }
          //use a simple delay for debounce
          delay(230);
        }
      }
      //set the wait varible to 0
      wait[0] = 0;
    }
  }
  //same thing, only simpler
  if (!(PINB & (1 << PB0))) {
    if (!wait[1]) {
      time = millis();
      wait[1] = 1;
    }
  }
  else {
    if (wait[1]) {
      if ((millis() - time2) > 150) {
        if ((transpose + 12) <= 127) {
          transpose += 12;
        }
      }
      wait[1] = 0;
    }
  }
  //start an ADC calculation and reset the ADC converion flag
  ADCSRA |= (1 << ADSC) | (1 << ADIF);
  //wait for the ADC convertion to end
  while (!(ADCSRA & (1 << ADIF)));
  //do the same thing that we did in the getadc(); function
  int adc = ADC;
  if ((adc < (prev_read + noise)) && (adc > (prev_read - noise))) {
    count++;
  }
  else {
    count = 0;
    prev_read = adc;
  }
  if (count == 40) {
    count = 0;
    adc_real_read = adc;
  }

  for (int a = 0; a != 20; a++) {
    //check if the ADC value is in the range of the corresponding value of the note
    if ((adc_real_read < (voltage[a] + noise)) && (adc_real_read > (voltage[a] - noise))) {
      //if this is the first time we send a note on command to this note continue
      if (a != note_send) {
        //if we have not set a note off to the prev note set it right not
        if (note)midi.sendNoteOff(note);
        //send a note on of this note + transpose
        midi.sendNoteOn(a + transpose, 127);
        note = a + transpose;
        note_send = a;
      }
    }
  }
  //if a note has been realease
  if (adc_real_read > 950 && note) {
    //send a note off command
    midi.sendNoteOff(note);
    note = 0;
    note_send = 255;
  }
}

