// RTOS Framework - Fall 2017
// J Losh

// Student Name:SHIKHAR TANDON
// TO DO: Add your name on this line.  Do not include your ID number.

// Submit only two .c files in an e-mail to me (not in a compressed file):
// xx_rtos_coop.c   Single-file with cooperative version of your project
// xx_rtos_prempt.c Single-file with premptive version of your project
// (xx is a unique number that will be issued in class)
// Please do not include .intvecs section in your code submissions
// Please do not change any function names in this code or thread priorities

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// 5 Pushbuttons and 5 LEDs, UART

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"

// REQUIRED: correct these bitbanding references for the off-board LEDs
#define BLUE_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) // on-board blue LED//PF2
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 0*4))) // off-board red LED//PD0
#define ORANGE_LED   (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 1*4))) // off-board orange LED//PD1
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 2*4))) // off-board green LED//PD2
#define YELLOW_LED   (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 3*4))) // off-board yellow LED//PD3

#define PB1      (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 6*4))) //PD6
#define PB2      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 7*4))) //PC7
#define PB3      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4))) //PC6
#define PB4      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4))) //PC5
#define PB5      (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4))) //PC4


//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// function pointer
typedef void (*_fn)();
void* findSP(int*);
void* findSP1(int);
void waitMicrosecond(uint32_t);

char* getString();
void ParseString(char*,int[],int);
bool isCommand(char*,int);

// semaphore
#define MAX_SEMAPHORES 5
#define MAX_QUEUE_SIZE 5
struct semaphore
{
  uint16_t count;
  uint16_t fix_count;
  uint16_t queueSize;
  uint32_t processQueue[MAX_QUEUE_SIZE]; // store task index here of waiting processes
  uint16_t owner;
} semaphores[MAX_SEMAPHORES];
uint8_t semaphoreCount = 0;

struct semaphore *keyPressed, *keyReleased, *flashReq, *resource ,*previous;

// task
#define STATE_INVALID    0 // no task
#define STATE_UNRUN      1 // task has never been run
#define STATE_READY      2 // has run, can resume at any time
#define STATE_BLOCKED    3 // has run, but now blocked by semaphore
#define STATE_DELAYED    4 // has run, but now awaiting timer

#define MAX_TASKS 10       // maximum number of valid tasks
uint8_t taskCurrent ;   // index of last dispatched task
uint8_t taskCount = 0;     // total number of valid tasks

#define MAX_CHARS 20
int offset_i;
int field_offset[20];
char* str1;
char str[MAX_CHARS];

uint64_t start,stop,del_ticks;
float sum_ticks=0,previ;
float avg;
uint64_t percent;

int instance = 0,prev;

char proc_name[20];


struct _tcb
{
  uint8_t state;                 // see STATE_ values above
  uint8_t skip;					//skip count for priority scheduling
  void *pid;                     // used to uniquely identify thread
  void *pid1;					//to store original pid if thread is destroyed
  void *sp;                      // location of stack pointer for thread
  uint8_t priority;              // 0=highest, 15=lowest
  uint8_t currentPriority;       // used for priority inheritance
  uint32_t ticks;                // ticks until sleep complete
  float ticks2;
  float previ;
  float avg;
  char name[16];                 // name of task used in ps command
  void *semaphore;               // pointer to the semaphore that is blocking the thread
} tcb[MAX_TASKS];


uint32_t stack[MAX_TASKS][256];

struct _tcb *sema[MAX_TASKS];
int u;
float s1;



//-----------------------------------------------------------------------------
// RTOS Kernel Functions
//-----------------------------------------------------------------------------



void rtosInit()
{
  uint8_t i;
  // no tasks running
  taskCount = 0;
  // clear out tcb records
  for (i = 0; i < MAX_TASKS; i++)
  {
    tcb[i].state = STATE_INVALID;
    tcb[i].pid = 0;
  }
  NVIC_ST_CTRL_R |= 0;
  NVIC_ST_RELOAD_R |= 0x00009C3F; //9C3Fh==39999SysTick_Config(4000000 / 1000)-1;//40Mhz clock with a 1ms system timer
  NVIC_ST_CURRENT_R |= 0;
  NVIC_ST_CTRL_R |= 0x00000007;//enable systick counter and interrupt
  // REQUIRED: initialize systick for 1ms system timer
   //SysTick_Config(4000000 / 1000);//40Mhz clock with a 1ms system timer
}


void rtosStart()
{
	int *sp1;
  // REQUIRED: add code to call the first task to be run
  _fn fn;
  taskCurrent = rtosScheduler();
  // Add code to initialize the SP with tcb[task_current].sp;
  //tcb[task_current].sp = stack[taskCurrent][255]
  //sp1 = findSP(taskCurrent);
  sp1 = tcb[taskCurrent].sp;//get the position of stack pointer
  __asm("             MOV  R13, R0");
  fn = tcb[taskCurrent].pid;
  tcb[taskCurrent].state = STATE_READY;

  (*fn)();

}


void* findSP(int *current)//returns the value of SP
{
	__asm("             MOV  R0, R13");
	__asm("             ADD  R13, #8");
	__asm("             MOV  R0, R13");
}



void* findSP1(int current)//returns the value of tcb[taskCurrent].sp
{
    return tcb[current].sp;

}



int strcscmp(char arr1[],char arr2[])//check for case insensitivity
{
	int ind = 0,flag = 0;
	while(ind < strlen(arr1))
	{
		if(arr1[ind] == arr2[ind] || arr1[ind] == arr2[ind]+32 || arr1[ind] == arr2[ind]-32)
			flag = 1;
		else
		{
			flag = 0;
			break;
		}
		ind++;
	}
	return flag;
}


bool createThread(_fn fn, char name[], int priority)
{
  bool ok = false;
  uint8_t i = 0,len = 0,num1 =0,got = 0;
  bool found = false;
  // REQUIRED: store the thread name
  // add task if room in task list
  if (taskCount < MAX_TASKS)
  {
    // make sure fn not already in list (prevent reentrancy)
    while (!found && (i < MAX_TASKS))
    {
       found = (tcb[i++].pid ==  fn);//function pointer will not be equal to pid
       if(found)
       {
    	   found = (tcb[i++].pid ==  fn);//function pointer will not be equal to pid.
       }
    }
    while(num1 < MAX_TASKS)//giving the correct pid and state if running again after killing
    {
    	if(strcscmp(tcb[num1].name,name)==1)
    	{
    		got = 1;
    		tcb[num1].state = STATE_UNRUN;//task has never been run
    		tcb[num1].pid = fn;//POINTER TO TASK
    	}
    	num1++;
    }

    if (!found && got == 0)//run only when create thread is called for creating new processes
    {
      // find first available tcb record
      i = 0;
      len = 0;
      while (tcb[i].state != STATE_INVALID) {i++;}//TASK FIND
      tcb[i].state = STATE_UNRUN;//task has never been run
      tcb[i].pid = fn;//POINTER TO TASK
      tcb[i].pid1 = fn;//POINTER TO TASK
      tcb[i].sp = &stack[i][255];//BASE ADDRESS OF FN
      tcb[i].priority = priority;
      tcb[i].currentPriority = priority;
      while(len < strlen(name))
      {
    	  tcb[i].name[len] = name[len];
    	  len++;
      }
      // increment task count
      taskCount++;
      ok = true;
    }
  }
  // REQUIRED: allow tasks switches again
  return ok;
}


void recreate(char fn_name[])//"<function_name> &" command
{
	int index = 0;
	_fn fn1;
	while(index < MAX_TASKS)
	{
		if((strcscmp(fn_name,tcb[index].name)==1) && tcb[index].state == STATE_INVALID)
		{
			fn1 = tcb[index].pid1;//restore the original pid
			createThread(fn1, tcb[index].name, tcb[index].priority);
			//taskCount--;
		}
		index++;
	}
}


// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield()
{

	NVIC_INT_CTRL_R |=  1 << 28;//set pendSV interrupt
  // push registers, call scheduler, pop registers, return to new function


}



void putcUart0(char c)//function that writes a serial character when the UART buffer is not full else it yields
{
	while((UART0_FR_R & UART_FR_TXFF)!=0)//REFERENCE TAKEN--http://codegist.net/search/stm8%20uart%20eaxample/6
	{
		yield();
		continue;
	}
	UART0_DR_R = c;

}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
	uint8_t m;
     for (m = 0; m < strlen(str); m++)
	  putcUart0(str[m]);
}

char getcUart0()//function that returns with serial data once the buffer is not empty else it yields
{
	while((UART0_FR_R & UART_FR_RXFE)!=0)//http://codegist.net/search/stm8%20uart%20eaxample/6
	{
		yield();
		continue;
	}
	return UART0_DR_R & 0xFF;


}


void checkQ(struct semaphore *pSemaphore,int taskC)
{
	//check if the process is pending in the queue ,if there is remove it and shift other processes to left
		int y = 0,flg=0,link;
		while(y < pSemaphore->queueSize)
		{
			if(pSemaphore->processQueue[y] == taskC)//if the killed task is pending in the queue
			{
				link = y;
				if(pSemaphore->queueSize == 1 || y == pSemaphore->queueSize - 1)//1st or last element in queue
					flg = 1;
				else
				{
					while(link < pSemaphore->queueSize -1)//shift to left
					{
						pSemaphore->processQueue[link] = pSemaphore->processQueue[link+1];
						link++;
						flg = 1;
					}
			    }
			}
			if(flg == 1)
				break;
		y++;
		}
		if(flg == 1)
		{
			pSemaphore->queueSize -= 1;
			flg = 0;
		}
}

void checksem(struct semaphore *pSemaphore,int taskC)//Release any semaphore held by the thread
{
	int num = 0;
		if(pSemaphore->owner == taskC)
		{

			if(pSemaphore->count < pSemaphore->fix_count)
				pSemaphore->count++;
			if(pSemaphore->queueSize > 0  && pSemaphore->owner != pSemaphore->processQueue[0])//if task is killed while running or it has not self-blocked itself
			{
				pSemaphore->owner = pSemaphore->processQueue[0];
				tcb[pSemaphore->owner].state = STATE_READY;
				while(num < pSemaphore->queueSize - 1)//shift the queue to left
				{
					pSemaphore->processQueue[num] = pSemaphore->processQueue[num + 1];
					num++;
				}
				pSemaphore->queueSize--;
			}
			else
				pSemaphore->owner = 0;//no one has the semaphore
		}

}


// REQUIRED: modify this function to destroy a thread
void destroyThread(_fn fn)
{
				int y = 0,z = 0;
				while(y < MAX_TASKS)
				{
					if(tcb[y].pid == fn)
						break;
					else
						y++;
				}

				//Release any semaphore held by the thread
				checksem(resource,y);
				checksem(flashReq,y);
				checksem(keyPressed,y);
				checksem(keyReleased,y);

				//check if the process is pending in the queue ,if there is remove it and shift other processes to left
				checkQ(resource,y);
				checkQ(flashReq,y);
				checkQ(keyPressed,y);
				checkQ(keyReleased,y);
				tcb[y].state = STATE_INVALID;
				tcb[y].pid1 = tcb[y].pid;//store the original pid before destroying for re-creating the thread
				tcb[y].pid = 0;
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
		int y = 0;
	 	while(y < MAX_TASKS)
		{
			if(tcb[y].pid == fn)
				break;
			else
				y++;
		}
		tcb[y].priority = priority;
}

struct semaphore* createSemaphore(uint8_t count)
{
  struct semaphore *pSemaphore = 0;
  if (semaphoreCount < MAX_SEMAPHORES)
  {
    pSemaphore = &semaphores[semaphoreCount++];
    pSemaphore->count = count;
    pSemaphore->fix_count = count;//the original count of semaphore is stored here
  }
  return pSemaphore;
}



// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{

	tcb[taskCurrent].ticks = tick;
	tcb[taskCurrent].state = STATE_DELAYED;//state is delayed when sleeping
	NVIC_INT_CTRL_R |=  1 << 28;

	// push registers, set state to delayed, store timeout, call scheduler, pop registers,
  // return to new function (separate unrun or ready processing)
}

// REQUIRED: modify this function to wait a semaphore with priority inheritance
// return if avail (separate unrun or ready processing), else yield to scheduler using pendsv




void wait(struct semaphore *pSemaphore)
{

	static int temp;
	if(pSemaphore->count > 0)
	{

		temp = taskCurrent;
		pSemaphore->owner = temp;
		pSemaphore->count--;
		previous = pSemaphore;//RECORDING the semaphore used

	}

	else{
		if(pSemaphore->count < MAX_QUEUE_SIZE)//add to queue if count<=0
		{
			pSemaphore->processQueue[pSemaphore->queueSize] = taskCurrent;
			tcb[taskCurrent].state = STATE_BLOCKED;
			//PRIORITY INHERITANCE
			if(tcb[temp].priority > tcb[taskCurrent].priority && previous == pSemaphore)//if the process handling it currently has less importancethan the current task
			{
				tcb[temp].skip = tcb[taskCurrent].priority;//temporarily elevate its priority so that it finishes off quicker
				strcpy(proc_name,tcb[taskCurrent].name);//recording the current task name

			}

			pSemaphore->queueSize++;
			NVIC_INT_CTRL_R |=  1 << 28;//yield
		}
	}

}


// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(struct semaphore *pSemaphore)
{
	int i = 1,wait_proc;
	pSemaphore->count++;
	//PRIORITY INHERITANCE
	if(strcmp(tcb[taskCurrent].name,proc_name) == 0)//if priority inheritance happened
	{
		tcb[taskCurrent].skip = tcb[taskCurrent].priority;//restore the original skip count
	}

	if(pSemaphore->queueSize > 0)
		{
		    wait_proc = pSemaphore->processQueue[0];//first in the queue gets ready
		    pSemaphore->owner = wait_proc;
		    //pSemaphore->front++;
		    tcb[wait_proc].state = STATE_READY;
		    pSemaphore->count--;
		    while(i <= pSemaphore->queueSize)//shift items to left
		    {
		    	pSemaphore->processQueue[i - 1] = pSemaphore->processQueue[i];
		    	i++;
		    }
			pSemaphore->queueSize--;

		}
	NVIC_INT_CTRL_R |=  1 << 28;//yield
}


// REQUIRED: Implement prioritization to 16 levels
int rtosScheduler()
{
  bool ok;
  static uint8_t task = 0xFF;
  ok = false;
  while (!ok)
  {
    task++;//task becomes 0 when scheduler runs for the very first time
    if (task >= MAX_TASKS)
      task = 0;
    ok = ((tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN));
    if(tcb[task].skip != 0)//if skip count is not 0 then decrement it
    {
    	tcb[task].skip--;
    	ok = false;
    }
    else if(tcb[task].skip == 0)//if skip count is 0 then restore its skip count and schedule it.
    {
    	tcb[task].skip = tcb[task].priority;
    	ok &=true;
    }

  }
  //start = WTIMER5_TAV_R;
  return task;
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr()
{
	 int count_task = 0,time;
	 time = tcb[taskCurrent].ticks;
	while(count_task < MAX_TASKS)
	{
		if(tcb[count_task].state == STATE_DELAYED)//if task is sleeping start decreasing its ticks
		{
			 while(time >= 0)
			{
				 if(time == 0)//if sleep duration is complete make it READY
					{
						tcb[count_task].state = STATE_READY;
						break;
					}
				 time--;
			}
		}

		count_task++;
	}

	tcb[taskCurrent].ticks2++;
	tcb[taskCurrent].ticks2 = (tcb[taskCurrent].ticks2*0.8) /*+ (tcb[taskCurrent].previ*0.7)*/;



}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr()
{
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr()
{

	int *sp_1,*sp_2,*pc;
	uint32_t lr;
    //_fn fn;
    __asm("             ADD  R13, #24");//adjust SP by 6(negating the effect of 6 registers that are pushed upon entry in PendSV Isr)


	__asm(" PUSH {R4-R11}");



	tcb[taskCurrent].sp = (findSP(tcb[taskCurrent].sp));//tcb[current].sp<--SP(save SP)
	__asm("             SUB  R13, #8");


	sum_ticks += tcb[taskCurrent].ticks2;
	sum_ticks = ((sum_ticks*0.8) /*+ (0.7*previ)*/);
	tcb[taskCurrent].avg = (tcb[taskCurrent].ticks2/sum_ticks)*100;

	taskCurrent = rtosScheduler();


    if(tcb[taskCurrent].state == STATE_READY)//if the next state is READY
    {
    	sp_2 = findSP1(taskCurrent);//SP<--tcb[next].sp(restore SP)
        __asm("             MOV  R13, R0");
    		__asm(" POP {R4}");
    		__asm(" POP {R5}");
    		__asm(" POP {R6}");
    		__asm(" POP {R7}");
    		__asm(" POP {R8}");
    		__asm(" POP {R9}");
    		__asm(" POP {R10}");
    		__asm(" POP {R11}");

    }

    else if(tcb[taskCurrent].state == STATE_UNRUN)//if process is running for the first time
    {
    	sp_2 = findSP1(taskCurrent);//SP<--tcb[next].sp
    	__asm("             MOV  R13, R0");//update R13 for new task






    	 pc = (tcb[taskCurrent].pid);
    	 //pushing r0-3,xPSR,LR,PC,SP
    	 stack[taskCurrent][255] = 1 <<24;
    	 stack[taskCurrent][254] = (uint32_t)pc - 1;
    	 stack[taskCurrent][253] = 2;
    	 stack[taskCurrent][252] = 3;
    	 stack[taskCurrent][251] = 4;
    	 stack[taskCurrent][250] = 5;
    	 stack[taskCurrent][249] = 6;
    	 stack[taskCurrent][248] = 7;


    	 tcb[taskCurrent].sp =  &stack[taskCurrent][248];
    	 __asm("             MOV  R13, R0");



    	 tcb[taskCurrent].state = STATE_READY;

    }

   lr = 0xFFFFFFF9;//tell LR to exit pendSV exception
   __asm("             MOV  LR, R0");
   __asm("             BX LR");


}

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{   // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

	// Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;

	// Enable GPIO port A,C,D,F peripherals
    SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOA|SYSCTL_RCGC2_GPIOF|SYSCTL_RCGC2_GPIOC|SYSCTL_RCGC2_GPIOD;

	// Configure external LED
    GPIO_PORTD_DIR_R |= 0x0F;  // make bit 0,1,2,3 an outputs of PORTD
    GPIO_PORTD_DR2R_R |= 0x0F; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTD_DEN_R |= 0x0F;  // enable PD0,PD1,PD2,PD3 LED

	// Configure ON-BOARD LED
    GPIO_PORTF_DIR_R |= 0x04;  // PF2
    GPIO_PORTF_DR2R_R |= 0x04; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTF_DEN_R |= 0x04;  // enable PF2

	// Configure PUSH BUTTONS
    GPIO_PORTC_DIR_R |= 0x00;  // PC4,PC5,PC6,PC7 as inputs
    GPIO_PORTC_DR2R_R |= 0xF0; // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTC_DEN_R |= 0xF0;  // enable PC4,PC5,PC6,PC7
	GPIO_PORTC_PUR_R |= 0xF0;  // PC4,PC5,PC6,PC7 as pull-up inputs

	//configure PD6
	GPIO_PORTD_DIR_R |= 0x00; //PD6 as input
	GPIO_PORTD_DR2R_R |= 0x40;// set drive strength to 2mA (not needed since default configuration -- for clarity)
	GPIO_PORTD_DEN_R |= 0x40;//enable PD6
	GPIO_PORTD_PUR_R |= 0x40;//PD6 as pull-up input



	// Configure UART0 pins
	SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;         // turn-on UART0, leave other uarts in same status
	GPIO_PORTA_DIR_R |= 3;
	GPIO_PORTA_DEN_R |= 3;
	GPIO_PORTA_AFSEL_R |= 3;
	GPIO_PORTA_PCTL_R = GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;

	// Configure UART0 to 115200 baud, 8N1 format (must be 3 clocks from clock enable and config writes)
	    UART0_CTL_R = 0;                                 // turn-off UART0 to allow safe programming
		UART0_CC_R = UART_CC_CS_SYSCLK;                  // use system clock (40 MHz)
	    UART0_IBRD_R = 21;                               // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
	    UART0_FBRD_R = 45;                               // round(fract(r)*64)=45
	    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN; // configure for 8N1 w/ 16-level FIFO
	    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN; // enable TX, RX, and module



  // REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
  //           5 pushbuttons, and uart
}



// Approximate busy waiting (in units of microseconds), given a 40 MHz system clock
void waitMicrosecond(uint32_t us)
{
	                                            // Approx clocks per us
  __asm("WMS_LOOP0:   MOV  R1, #6");          // 1
  __asm("WMS_LOOP1:   SUB  R1, #1");          // 6
   __asm("             CBZ  R1, WMS_DONE1");   // 5+1*3
  __asm("             NOP");                  // 5
  __asm("             B    WMS_LOOP1");       // 5*3
  __asm("WMS_DONE1:   SUB  R0, #1");          // 1
  __asm("             CBZ  R0, WMS_DONE0");   // 1
  __asm("             B    WMS_LOOP0");       // 1*3
  __asm("WMS_DONE0:");                        // ---
                                                // 40 clocks/us + error
}



// REQUIRED: add code to return a value from 0-31 indicating which of 5 PBs are pressed
uint8_t readPbs()
{
	if(PB1 == 0)//button 0
		return 1;
	else if(PB2 == 0)//button 1
		return 2;
	else if(PB3 == 0)//button 2
		return 4;
	else if(PB4 == 0)//button 3
		return 8;
	else if(PB5 == 0)//button 4
		return 16;
	else
		return 0;

}

// ------------------------------------------------------------------------------
//  Task functions
// ------------------------------------------------------------------------------

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle()
{
  while(true)
  {

    ORANGE_LED = 1;
    waitMicrosecond(1000);
    ORANGE_LED = 0;
    waitMicrosecond(1000);
    yield();
  }
}

void idle2()
{
  while(true)
  {

    YELLOW_LED = 1;
    waitMicrosecond(100000);
    YELLOW_LED = 0;
    waitMicrosecond(100000);
    yield();
  }
}

void idle3()
{
  while(true)
  {

    BLUE_LED = 1;
    waitMicrosecond(1000000);
    BLUE_LED = 0;
    waitMicrosecond(1000000);
    yield();
  }
}

void flash4Hz()
{
  while(true)
  {
    GREEN_LED ^= 1;
    sleep(1000);
  }
}

void oneshot()
{
  while(true)
  {
    wait(flashReq);
    BLUE_LED = 1;
    sleep(1000);
    BLUE_LED = 0;
    waitMicrosecond(100000);
  }
}

void partOfLengthyFn()
{

  // represent some lengthy operation
  waitMicrosecond(1000);
  // give another process a chance to run
  yield();
}

void lengthyFn()
{
;
  uint16_t i;
  while(true)
  {
    wait(resource);
    for (i = 0; i < 40; i++)
    {
      partOfLengthyFn();
    }
    RED_LED ^= 1;
    post(resource);
  }
}

void readKeys()
{
  uint8_t buttons;
  while(true)
  {
    wait(keyReleased);
    buttons = 0;
    while (buttons == 0)
    {
      buttons = readPbs();
      yield();
    }
    post(keyPressed);
    if ((buttons & 1) != 0)//button 0
    {
      YELLOW_LED ^= 1;
      RED_LED = 1;
    }
    if ((buttons & 2) != 0)//button 1
    {
      post(flashReq);
      RED_LED = 0;
    }
    if ((buttons & 4) != 0)//button 2
    {
      createThread(flash4Hz, "Flash4hz", 0);
    }
    if ((buttons & 8) != 0)//button 3
    {
      destroyThread(flash4Hz);
	}
    if ((buttons & 16) != 0)//button 4
    {
      setThreadPriority(lengthyFn, 4);
    }
    yield();
  }
}

void debounce()
{
  uint8_t count;
  while(true)
  {
    wait(keyPressed);
    count = 10;
    while (count != 0)
    {
      sleep(10);
      if (readPbs() == 0)
        count--;
      else
        count = 10;
    }
    post(keyReleased);//now read keys can work
  }
}

void uncooperative()
{
  while(true)
  {
    while (readPbs() == 8)
    {
    }
    yield();
  }
}

void important()
{
	while(true)
	{
      wait(resource);
      BLUE_LED = 1;
      sleep(1000);
      BLUE_LED = 0;
      post(resource);
	}
}


/*void reverse(char *str, int len)
{
    int i=0, j=len-1, temp;
    while (i<j)
    {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }

}


int pow(int a ,int n)
{
	int i=0,temp;
	temp = a;
	while(i < n-1)
	{
		a = a*temp;
	}
	return a;
}

int intToStr(int x, char str[], int d)
{
    int i = 0;
    char* rvs;
    while (x > 0)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}


void conv_to_string(float temp ,int deci_plac)
{
		char res[10],*floats;
	    // Extract integer part
	    int ipart = (int)temp;

	    // Extract floating part
	    float fpart = temp - (float)ipart;

	    // convert integer part to string
	    int i = intToStr(ipart, res, 0);

	    // check for display option after point
	    if (deci_plac != 0)
	    {
	        res[i] = '.';  // add dot

	        // Get the value of fraction part upto given no.
	        // of points after dot. The third parameter is needed
	        // to handle cases like 233.007
	        fpart = fpart * pow(10, deci_plac);

	        intToStr((int)fpart, res + i + 1, deci_plac);
	    }

}*/








void ps()//ps command-->prints pid,process name,process state and %CPU time
{
	int index = 0;
	uint64_t a = 0;
	//float percent;
	char text[20],*proc_id;
	 putsUart0("\rPID \t Name \t State \t %CPU\r");
	 putsUart0("\r===== \t ===== \t ===== \t =====\r\r");
					  while(index < taskCount)
					  {

						  sprintf(text,"%p",tcb[index].pid);
						  //proc_id = (char)tcb[index].pid;
						  putsUart0(text);//prints pid
						  putsUart0("\t");
						  putsUart0(tcb[index].name);//prints name
						  putsUart0("\t");
						  if(tcb[index].state == 0)//prints  state
							  putsUart0("STATE_INVALID");
						  else if(tcb[index].state == 1)
							  putsUart0("STATE_UNRUN");
					      else if(tcb[index].state == 2)
							  putsUart0("STATE_READY");
					      else if(tcb[index].state == 3)
					          putsUart0("STATE_BLOCKED");

					      else
					          putsUart0("STATE_DELAYED");

						  putsUart0("\t");
						  //a = (tcb[index].ticks2*100);
						  percent = tcb[index].avg;
						  sprintf(text,"%d",percent);
						  putsUart0(text);
						  putsUart0("%");
						  putsUart0("\r");
						  index++;
					  }
}

void ipcs(struct semaphore *pSemaphore)//ipcs command-->prints semaphore name,queue size,queue count,owner of semaphore
{
	int xtra = 0,temp,len;
	static int indic = 0;
	char s_str[20];
	char* sphor_string;
	//struct semaphore *sPhores;
	if(indic == 0)
	{
		putsUart0("\rName \t Count \t QueueSize \t Owner of Semaphore\r");
		putsUart0("\r===== \t ===== \t ===== \t\t ========== \r\r");
	}

		if(indic == 0)//prints name of semaphore
			putsUart0("keyPressed");
		else if(indic == 1)
			putsUart0("keyReleased");
		else if(indic == 1)
			putsUart0("keyReleased");
		else if(indic == 2)
			putsUart0("flashReq");
		else
			putsUart0("resource");
		putsUart0("\t");

		temp = pSemaphore->count;
		sprintf(s_str,"%d",pSemaphore->count);
		putsUart0(s_str);//semaphore count
		putsUart0("\t");
		temp = pSemaphore->queueSize;
		sprintf(s_str,"%d",pSemaphore->queueSize);
		putsUart0(s_str);//semaphore queue size
		//putsUart0(sphor_string);
		putsUart0("\t");
		if(pSemaphore->owner != 0)//checks if somebody has a semaphore
		{
			sprintf(s_str,"%s",tcb[pSemaphore->owner].name);
			putsUart0(s_str);//semaphore owner
		}
		else
			putsUart0("None");
		putsUart0("\r");
		indic++;
		if(indic == 4)
		{
			indic = 0;

		}


}

void pidof(_fn fn)//tells the process id of thread
{
	int y = 0;
	char nam[20];
	while(y < MAX_TASKS)
	{
		if(tcb[y].pid == fn)
		break;
		else
			y++;
	}
	sprintf(nam,"%p",tcb[y].pid);
	putsUart0(nam);
}

void kill(_fn fn)//calls the destroy function to kill the process
{
	int y = 0;
	while(y < MAX_TASKS)
	{
		if(tcb[y].pid == fn)
		{
			break;
		}
		else
			y++;
	}
	destroyThread(tcb[y].pid);
	//tcb[y].pid1 = tcb[y].pid;//store the original pid before destroying for re-creating the thread
	//tcb[y].pid = 0;
}

void shell()//we get user`s input in here
{

	while (true)
	{
			putsUart0("\rEnter the command:");
			str1=getString();//gets the string
			ParseString(str1,field_offset,offset_i);
			if(isCommand("ps",0)==1)//calls isCommand
				  {


				  }

			else if(isCommand("ipcs",0)==1)//calls isCommand
				  {


				  }

			else if(isCommand("reboot",0)==1)//calls isCommand
				  {


				  }

			else if(isCommand("kill",1)==1)//calls isCommand
				  {

				  }

			else if(isCommand("pidof",1)==1)//calls isCommand
				  {

				  }

			else if(isCommand("&",1)==1)//recreate a function
				 {

				 }

			else
			{
				putsUart0("\rWrong Command");
				yield();
			}

				// REQUIRED: add processing for the shell commands through the UART here


	  }
}

//-----------------------------------------------------------------------------
// YOUR UNIQUE CODE
// REQUIRED: add any custom code in this space
//-----------------------------------------------------------------------------
char* getString()
{

    //first=true;

	int count=0;
	offset_i=0;


	while(count < MAX_CHARS)
	      {

	      char ch= getcUart0();
	      if(ch==10)
	    	  continue;
	      if(ch=='\b'){//if BACKSPACE
	    	  if(count>0)
	    	  {
	              if(count-1==field_offset[offset_i-1])//if character at field_offset index is to be backspaced
	              {
	            	  field_offset[offset_i-1]='\0';
	            	  offset_i=offset_i-1;
	              }

	    		 //necessary backspace
	    		  {
	    			  count-=1;
	  			      str[count]='\0';
	  		      }

	    		  continue;
	    	  }
	    	  else{//ignore backspace if nothing written

	    		  continue;
	    	  }
	      }
	      else if(ch=='\r')//if CARRIAGE RETURN
	      {
	    	  str[count]=ch;
	    	  str[count]='\0';//if CARRIAGE RETURN -->END OF COMMAND
              if((str[count-1]==' ' || str[count-1]==';' || str[count-1]==':' || str[count-1]==',' || str[count-1]=='\t' ))
	    	  {
            	  while(str[count-1]==' ' || str[count-1]==';' || str[count-1]==':' || str[count-1]==',' || str[count-1]=='\t')//ignoring the delimiters before the \r character
	    		  {
            		  str[count-1]='\0';
            		  count-=1;
            	  }
              }
	    	  break;//end of command when enter is pressed
	      }
	      else if(ch==' '|| ch==';' || ch==':' || ch=='\t' || ch==',')//if character is a delimiter

	      {
	    	  str[count]=ch;
	           count+=1;
	    	  continue;
	      }
	      else //if normal character
	      {
	           if(str[count-1]==' ' || str[count-1]==',' || str[count-1]=='\t' || str[count-1]==':' || str[count-1]==';')//if previous character was a delimiter
	            {

	    	  str[count]=ch;//store character in array
	    	  field_offset[offset_i]=count;//count as a field offset
	    	  offset_i++;

	    	  //flag=0;
	    	  //probe=0;
	    	    }
	    	  //END OF CHARACTER INPUT

	           else//CHECK FOR CASE INSENSITIVITY
	           {
	        	   if(ch>='A' && ch<='Z')
	        		   str[count]=(ch)+32;
	        	   else if(ch>='a' && ch<='z')
	        		   str[count]=ch;
	        	   else if(ch>='0' && ch<='9')
	        		   str[count]=ch;

	           }
	      }

	      if(count == 0)//if first character is not a delimiter
	      {
	    	  if(str[0]!=' ' || str[0]!=',' || str[0]!=':' || str[0]!=';' || str[0]!='\t')
	    	             {
	    		          field_offset[offset_i]=count;
	    		          offset_i++;
	    	             }
	      }
	      count+=1;
	    }



	   putsUart0(str);
	   putsUart0(" Command Entered");
	   putsUart0("\r");


	      return str;

}

/////////////////////////////////////////////////////PARSE THE STRING///////////////////////////////////////////////////
void ParseString(char *str,int field_offset[],int i)
{
	int t=0;
	int q;


	while(t<i)//putting NULL at every delimiter position
	   {
		  q=1;
		 while(str[field_offset[t]-q]==' ' || str[field_offset[t]-q]==',' || str[field_offset[t]-q]==':' ||str[field_offset[t]-q]==';' || str[field_offset[t]-q]=='\t')   //till all delimiters are made NULL

		  {

		   str[field_offset[t]-q]='\0';
		   q++;

		  }
		   t=t+1;
	   }
}

///////////////////////////////////////////////////////IS_COMMAND//////////////////////////////////////////////////////
_Bool isCommand(char* command,int ArgCount)
{
	int flag = 0;
	int index = 0;
	void* p;
	char txt[20];

	if(strcscmp(command,&str1[field_offset[1]])==1 && ArgCount < offset_i)//for the "function_name &" command
		{
			sscanf(&str1[field_offset[0]], "%s", txt);//REFERENCE TAKEN--https://stackoverflow.com/questions/4530822/c-c-converting-memory-address-string-back-to-a-pointer
			index = 0;
				while(index < taskCount)
					{
						if(strcscmp(tcb[index].name,txt)==1)
						{
							recreate(txt);
							flag = 1;
							break;
						}
					index++;
					}
		}

    if(strcscmp(command,&str1[field_offset[0]])==1 && ArgCount < offset_i)//for calling ps,ipcs,kill,reboot and pidof
    {
		if(strcscmp("ps",&str1[field_offset[0]])==1)//ps
		  {
			  ps();
			  flag = 1;
		  }

		else if(strcscmp("ipcs",&str1[field_offset[0]])==1)//ipcs
		  {

			    ipcs(keyPressed);
			    ipcs(keyReleased);
			    ipcs(flashReq);
			    ipcs(resource);
			    flag = 1;
		  }

		else if(strcscmp("reboot",&str1[field_offset[0]])==1)//reboot
		  {
			  ResetISR();//resets the full hardware
		  }


		else if(strcscmp("kill",&str1[field_offset[0]])==1)//kill
			  {
				sscanf(&str1[field_offset[1]], "%p", (void **)&p);//REFERENCE TAKEN--https://stackoverflow.com/questions/4530822/c-c-converting-memory-address-string-back-to-a-pointer
				while(index < taskCount)
				{
					if(tcb[index].pid == p)
					{
						kill((tcb[index].pid));
						flag = 1;
						break;
					}
					index++;
				}

			  }

		else if(strcscmp("pidof",&str1[field_offset[0]])==1)//pidof
			{
				sscanf(&str1[field_offset[1]], "%s", txt);//REFERENCE TAKEN--https://stackoverflow.com/questions/4530822/c-c-converting-memory-address-string-back-to-a-pointer
				index = 0;
				while(index < taskCount)
				{
					if(strcscmp(tcb[index].name,txt)==1)
					{
						pidof(tcb[index].pid);
						flag = 1;
						break;
					}
				index++;
				}

			}
    }



	if(flag == 1)//if it executed any command
		return 1;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
 {
  bool ok = true;
  char* str1;
  // Initialize hardware

  initHw();



  rtosInit();

  // Power-up flash
  RED_LED = 1;
  waitMicrosecond(250000);
  RED_LED = 0;
  waitMicrosecond(250000);


  // Initialize semaphores
  keyPressed = createSemaphore(1);
  keyReleased = createSemaphore(0);
  flashReq = createSemaphore(5);
  resource = createSemaphore(1);

  // Add required idle process
  ok &=  createThread(idle, "Idle", 15);


  // Add other processes
  ok = createThread(lengthyFn, "LengthyFn", 12);
  ok &= createThread(flash4Hz, "Flash4hz", 4);
 ok &= createThread(oneshot, "OneShot", 4);
  ok &= createThread(readKeys, "ReadKeys", 8);
  ok &= createThread(debounce, "Debounce", 8);
  ok = createThread(important, "Important", 0);
  ok &= createThread(uncooperative, "Uncoop", 10);
  ok &= createThread(shell, "Shell", 8);

  // Start up RTOS
  if (ok)
    rtosStart(); // never returns
  else
    RED_LED = 1;

return 0;
  // don't delete this unreachable code
  // if a function is only called once in your code, it will be
  // accessed with two goto instructions instead of call-return,
  // so any stack-based code will not function correctly
  yield(); sleep(0); wait(0); post(0);
}

