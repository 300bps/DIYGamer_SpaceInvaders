// ## Space Invaders ##
//
// Arduino Hardware: "DIY Gamer V1.9" made by "Technology Will Save Us" (as seen at https://www.adafruit.com/product/2123)
// Arduino Libraries Required: Gamer
// Note: To install library, select Sketch->Include Library->Manage Libraries...  and select the "Gamer" library.
//
// By: David and Brett Smith
// 11/26/19

#include "Gamer.h"

#define FALSE                 0
#define TRUE                  (!FALSE)

#define BOARD_MIN_X           0     // Board min x coord
#define BOARD_MIN_Y           0
#define BOARD_MAX_X           7     // Board max x coord
#define BOARD_MAX_Y           7

#define NUM_ALIENS_TO_START   9     // Initial number of aliens
#define MAX_ALIENS_PER_ROW    3     // Initial number of aliens per row

#define PLAYERSTART_X         3     // Player starting column
#define PLAYERSTART_Y         7     // Player starting row
#define MAX_ACTIVE_PLAYER_LASERS    2   // Number of player lasers allowed in play at one time

#define MAX_NUM_DRAW_COUNTS   6     // Number of states before 'draw_count' restarts
#define MAX_LASER_DRAW_COUNTS 3     // Number of states before laser draw updates (controls move speed of lasers)
#define GAMELOOP_INTERVAL_MS  100   // Number of milliseconds per gameloop/display updates
#define GAMESTART_DELAY_MS    2000  // Delay to allow player to prepare before game begins

// Enumeration represents type of objects on game board
enum gameitem_enum 
{
  eEmpty = 0,
  eAlien = 1,
  eLaser = 2,
  eAlienLaser = 3,
  ePlayer = 4
};

enum button_enum
{
  eButton_none = 0,
  eButton_down = 1,
  eButton_up = 2,  
  eButton_left = 3,
  eButton_right = 4,
  eButton_fire = 5,
  eButton_start = 6
};

enum move_result_enums
{
  eMove_horizontal = 0,
  eMove_advanced = 1,
  eMove_playerboom = 2,
  eMove_alienslanded = 3
};



// Handle to the gamer library
Gamer gamer;

// The gameboard
uint8_t gameboard[8][8] = {0};    // Store the gameitem_enums representing what is in each space on the board
uint8_t temp_board[8][8] = {0};   // Used to temporarily store new positions during aliens move
byte    drawbuffer[8];            // Buffer for rendering to display.  Each element is a byte for the row with each bit representing the column (ie {b0101010, b10101010, ...} <- 1st element is row 0 with msb col 0.
uint8_t scrolltext_buffer[20];    // Buffer to hold text to scroll to display

bool    game_over = FALSE;
byte    game_level = 0;           // Number of levels (waves complete)
byte    draw_count = 0;           // Used to control animate speed based on game level
byte    player_x, player_y;       // Player's x position (column); Player's y position (row)
byte    active_player_lasers;     // Number of player lasers on board at once

byte    player_score = 0;         // Track the number of alien's killed
byte    live_alien_count = 0;     // Tracks the number of live aliens on the gameboard
int8_t  alien_move_step = 1;      // Alternates between +1/-1 to indicate the aliens direction of motion (+1 = right)
byte    alien_move_counter = 0;   // Counts individual frames of 'GAMELOOP_INTERVAL_MS' to control move speed of aliens
byte    laser_move_counter = 0;   // Counts individual frames of 'GAMELOOP_INTERVAL_MS' to control move speed of lasers
byte    game_tone_counter = 0;    // Counter used to synch sound effects with the alien moves


// Function pointer to reset the game by calling reset_game()
void (*reset_game)(void) = 0;


// Load the gameboard with the specified number of aliens
void load_aliens(byte num_aliens)
{
  uint8_t row, col, count, aliens_per_row;
  bool alien_spot;
  
  for (row = 0; row < 8; row++)
    for (col = 0, aliens_per_row = 0; col < 8; col++)
    {
        // Odd numbered rows offset the aliens 1 column, so skip the first column on odd rows
        if ((col == 0) && (row % 2 != 0))
          continue;
               
        count = row*8 + col;
        if (num_aliens > 0)
        {
          alien_spot = FALSE;
          
          if (row % 2 == 0)
          {
            // Even rows, aliens start column 0
            if (count%2==0)
            {
              alien_spot = TRUE;              
            }
          }
          else
          {
            // Odd rows, aliens start column 1
            if (count%2==1)
            {
              alien_spot = TRUE;
            }
            
          }

          if (alien_spot && (aliens_per_row < MAX_ALIENS_PER_ROW))
          {
            gameboard[row][col] = eAlien;
            num_aliens--;
            aliens_per_row++;
          }      
      }  
    }
}


// Render the current contents of the gameboard.
void draw_gameboard(byte draw_count)
{
  uint8_t row, col, render_row;
  uint8_t temp;
  
  // Populate render buffer from gameboard
  for (row = 0; row < 8; row++)
  {
    render_row = 0;  // Start with empty row
    for (col = 0; col < 8; col++)
    {
      temp = gameboard[row][col];
      
      if (temp == eEmpty)
        // If spot is empty, nothing to draw here
        continue;
      else
      {
        // Handle animations for specific items
        if (temp == eLaser)
        { 
          // Handle flashing of the laser
          if (draw_count % 2 == 0)          
            render_row |= 1<<(7-col);
        }
        // For all others, ...
        else
        {
          // Draw solid pixel
          render_row |= 1<<(7-col);
        }
      }
    }
    
    // Update draw buffer with the rendered row pattern
    drawbuffer[row] = render_row;
  }

  // Draw it
  gamer.printImage(&drawbuffer[0]);  
}


// Move lasers on the gameboard
move_result_enums move_lasers(void)
{
  byte row, col;
  
  for (row = BOARD_MIN_Y; row <= BOARD_MAX_Y; row++)
  {
    for (col = BOARD_MIN_X; col <= BOARD_MAX_X; col++)
    {
      if (gameboard[row][col] == eAlienLaser)
      {
        // Check if goes off board
        if (row == BOARD_MAX_Y)
          gameboard[row][col] = eEmpty;
        else 
        {
          // If spot below is empty, move laser to it
          if (gameboard[row+1][col] == eEmpty)
          {
            gameboard[row][col] = eEmpty;
            gameboard[row+1][col] = eAlienLaser;
          }
          // If spot below has alien, blow it up
          else if (gameboard[row+1][col] == eAlien)
          {
            gameboard[row][col] = eEmpty;
            gameboard[row+1][col] = eEmpty;
            live_alien_count--;            
            player_score++;
          }
          else if (gameboard[row+1][col] == ePlayer)
            return eMove_playerboom;
        }
      }
      else if (gameboard[row][col] == eLaser)
      {
        // Check if goes off board
        if (row == BOARD_MIN_Y)
        {
          gameboard[row][col] = eEmpty;
          active_player_lasers--;
        }
        else
        {        
          // If spot above is empty, move laser to it
          if (gameboard[row-1][col] == eEmpty)
          {
            gameboard[row][col] = eEmpty;
            gameboard[row-1][col] = eLaser;
          }
          // If spot above has alien, blow it up
          else if (gameboard[row-1][col] == eAlien)
          {
            gameboard[row][col] = eEmpty;
            gameboard[row-1][col] = eEmpty;
            active_player_lasers--;
            live_alien_count--;            
            player_score++;
          }
        }
      }      
    }
  }  
  return eMove_advanced;
}


// Move all of the aliens
move_result_enums move_aliens(int8_t step_value)
{
  bool advance;  
  int8_t row, col, new_row, new_col, temp;

  // Scan board to see if aliens move horizontally or advance
  advance = FALSE;
  for (row = BOARD_MIN_Y; row <= BOARD_MAX_Y; row++)
  {
    for (col = BOARD_MIN_X; col <= BOARD_MAX_X; col++)
    {
      // Init the temp board space while walking through the array
      temp_board[row][col] = eEmpty;
      
      // Check for aliens
      if (gameboard[row][col] == eAlien)
      {
        // If move would result in alien moving outside of visible area, then advance
        if ((col + step_value > BOARD_MAX_X) || (col + step_value < BOARD_MIN_X))
          advance = TRUE;
      }
    }
  }

  // Move the aliens either side to side or advance a row
  for (row = BOARD_MIN_Y; row <= BOARD_MAX_Y; row++)
  {
    for (col = BOARD_MIN_X; col <= BOARD_MAX_X; col++)
    {
      // Check for aliens      
      if (gameboard[row][col] == eAlien)
      {
        if (advance)
        {
          // Advancing
          new_row = row + 1;
          new_col = col;
        }
        else
        {
          // Moving horizontally
          new_row = row;
          new_col = col + step_value;
        }
        
        // Check destination
        temp = gameboard[new_row][new_col];
        
        // If empty, make the move
        if (temp == eEmpty)
        {
          gameboard[row][col] = eEmpty;            
          temp_board[new_row][new_col] = eAlien;
        }
        else if (temp == ePlayer)
        {
          // ** GAME OVER **
          return eMove_playerboom;  
        }
        else if (temp == eLaser)
        {
          // Alien and laser destroyed
          gameboard[row][col] = eEmpty;
          gameboard[new_row][new_col] = eEmpty;          
          active_player_lasers--;
          live_alien_count--;
          player_score++;
        }

        // If the aliens have landed, player loses
        if ((new_row == BOARD_MAX_Y) && (temp_board[new_row][new_col] == eAlien))
          return eMove_alienslanded;        
      }
    }
  }

  // Update the gameboard from the temp board
  for (row = BOARD_MIN_Y; row <= BOARD_MAX_Y; row++)
  {
    for (col = BOARD_MIN_X; col <= BOARD_MAX_X; col++)
    {
      // Transfer the aliens from their temp board new locations to actual board
      if (temp_board[row][col] == eAlien)
        gameboard[row][col] = eAlien;      
    }
  }


  // If the aliens have advanced, but not won, signal so game loop can change alien step direction
  if (advance)
    return eMove_advanced;

  // The aliens moved horizontally
  return eMove_horizontal;
}


// Scan the buttons and return the highest priority button pressed
button_enum read_inputs(byte draw_count)
{
  // Check fire button - highest priority
  if (gamer.capTouch() || gamer.isPressed(UP) || gamer.isPressed(DOWN))
    return eButton_fire;

  // Check other buttons in order of priority
  if (gamer.isPressed(RIGHT))
    return eButton_right;

  if (gamer.isPressed(LEFT))
    return eButton_left;

  if (gamer.isPressed(START))
    return eButton_start;

  // No button was pressed
  return eButton_none;
}


// Clear the gameboard.
void clear_gameboard(void)
{
  for (byte row = BOARD_MIN_Y; row < BOARD_MAX_Y; row++) 
  {
    for (byte col = BOARD_MIN_X; col < BOARD_MAX_X; col++)
    {
      gameboard[row][col] = eEmpty;
      
    }
  }
}


void setup() {
  // put your setup code here, to run once:

  // Init serial port to send debug output to terminal
  Serial.begin(9600);
  
  gamer = Gamer();
  gamer.begin();

  // Add aliens to the game board
  load_aliens(NUM_ALIENS_TO_START);
  live_alien_count = NUM_ALIENS_TO_START;
  alien_move_step = 1;

  // Add player
  player_x = PLAYERSTART_X;
  player_y = PLAYERSTART_Y;
  active_player_lasers = 0;

  // Init move update counters
  alien_move_counter = 0;
  laser_move_counter = 0;
  game_tone_counter = 0;

  // Init counter used to track number of completed waves and move speed
  game_level = 0;
  player_score = 0;

  // Read and ignore pushbuttons to init 
  // (found that it was returning a press for each physical button on startup)
  read_inputs(eButton_up);
  read_inputs(eButton_down);
  read_inputs(eButton_left);
  read_inputs(eButton_right);

  // Title Screen
  gamer.printString("Space Invaders");
  delay(1000);
  gamer.printString("Wave 1");
  delay(500);
  
  // Draw initial game board
  draw_gameboard(0);
  delay(GAMESTART_DELAY_MS);
}



// Note: this loop gets called repeatedly by the system
void loop() {  
  button_enum   button;
  int8_t        iresult;
  

  // Handle GAME OVER
  while (game_over)
  {
    gamer.playTone(180);
    gamer.allOn();
    delay(250);
    
    gamer.playTone(200);
    gamer.clear();
    delay(250);    
    
    gamer.playTone(220);
    gamer.allOn();
    delay(250);    

    gamer.playTone(250);
    gamer.clear();
    delay(500);    

    gamer.stopTone();
    gamer.printString("Game Over");
    delay(1000);

    gamer.printString("Score:");
    delay(500);
    gamer.showScore(player_score);
    delay(2000);

    // Restart the game
    reset_game();
  }


  // If all aliens killed, level up
  if (live_alien_count == 0)
  {
    // Blank the display
    clear_gameboard();
    
    // Reload aliens
    load_aliens(NUM_ALIENS_TO_START);
    live_alien_count = NUM_ALIENS_TO_START;
    alien_move_step = 1;    

    // Reinit variables
    active_player_lasers = 0;
    alien_move_counter = 0;
    laser_move_counter = 0;
    game_tone_counter = 0;

    // Increase game level (which increases alien move speed)
    game_level++;

    // Display level
    delay(1000);
    sprintf(scrolltext_buffer, "Wave %d", game_level + 1);
    gamer.printString(scrolltext_buffer);
    delay(500);

    // Update the display 
    draw_count = 0;    
    draw_gameboard(0);

    // Delay to give player chance to prepare
    delay(GAMESTART_DELAY_MS);
  }


  // Scan player input
  button = read_inputs(draw_count);


  // ** MOVE PLAYER SHIP **
  
  // Remove player's "current" position  
  gameboard[player_y][player_x] = eEmpty;

  // Handle left/right button press
  if (button == eButton_left)
  {
    // Move left if it will be still on gameboard
    if (player_x > BOARD_MIN_X)
      player_x--;
  }
  else if (button == eButton_right)
  {
    // Move right if it will be still on gameboard
    if (player_x < BOARD_MAX_X)
      player_x++;
  }

  // If player has moved into or been hit by laser, end game
  if ((gameboard[player_y][player_x] == eAlien) || (gameboard[player_y][player_x] == eLaser))
  {
    // Set game over flag and exit so that loop with run again and hit the game over code
    game_over = TRUE;
    return;
  }
  else
    // ELSE, update gameboard with player's "new" position
    gameboard[player_y][player_x] = ePlayer;


  // ** LAZER FIRED **
  // Move existing lasers
//  if (laser_move_counter++ >= (MAX_NUM_DRAW_COUNTS - game_level))
  if (laser_move_counter++ >= MAX_LASER_DRAW_COUNTS)
  {  
    // Reset move counter
    laser_move_counter = 0;

    // Move the lasers and check for game end
    if (move_lasers() == eMove_playerboom)
    {
      game_over = TRUE;
      return;
    }
  }
  
  
  // Lazer fired?
  if (button == eButton_fire && active_player_lasers < MAX_ACTIVE_PLAYER_LASERS)
  {
    if (gameboard[player_y-1][player_x] == eAlien)
    {
        // Alien killed
        gameboard[player_y-1][player_x] = eEmpty;
        live_alien_count--;
        player_score++;
    }
    else if (gameboard[player_y-1][player_x] == eEmpty)
    {
      gameboard[player_y-1][player_x] = eLaser;
      active_player_lasers++;
    }
  }


  // ** Move aliens **
 // Decide when (how fast) to move aliens based on current game level
  if (alien_move_counter++ >= (MAX_NUM_DRAW_COUNTS - game_level))
  {

    // Alternate the tones each time the aliens move
    if (game_tone_counter++ % 2 == 0)    
      gamer.playTone(210);
    else
      gamer.playTone(250);
    
    // Reset alien move counter
    alien_move_counter = 0;

    // Move aliens are check result
    iresult = move_aliens(alien_move_step);  
    if (iresult == eMove_advanced)
      alien_move_step = 0 - alien_move_step;      // Reverse direction
    else if ((iresult == eMove_playerboom) || (iresult == eMove_alienslanded))
    {
      // Set game over flag and exit so that loop with run again and hit the game over code    
      game_over = TRUE;
      return;
    }
  }


  // Update the display from the game board
  draw_gameboard(draw_count);


  delay(GAMELOOP_INTERVAL_MS);
  gamer.stopTone();

  // Circularly increment draw counter
  draw_count = (draw_count + 1) % MAX_NUM_DRAW_COUNTS;
}
