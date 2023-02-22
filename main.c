/**
  ******************************************************************************
  * @file    Project/main.c 
  * @author  L
  * @version V4.0
  * @date    24-January-2022
  * @brief   Main program body
  ******************************************************************************
InPut:
InputPressure -------->PD2;
Water_Signal  -------->PD1;   桶水压大于2.5kg为0（断开），小于1.5kg为1（闭合）
BucketPressure-------->PC3;

OutPut:
VALVE_IN      ----------->PC4;  原水到水泵的进水阀
PUMP          ----------->PD4;
VALVE_RO_IN   ----------->PB4;  RO膜冲洗进水阀
VALVE_RO_OUT  ----------->PB5;  RO膜冲洗出水阀

ErrorFlag------->bits: 7 6 5 4 3 2 1 0  故障置1，正常为0
bit 7 制水时发生停水/水压波动 置0；


*/ 
#include"stm8s.h"
//#include"stm8s_conf.h"

#include"stm8s_gpio.h"
#include"stm8s_clk.h"
#include"stm8s_tim1.h"
#include"stm8s_tim2.h"
/*  ---------------------------------------------------------*/
#define AlarmOn      GPIO_WriteHigh(GPIOA, GPIO_PIN_3)
#define AlarmOff     GPIO_WriteLow(GPIOA, GPIO_PIN_3)

#define VALVE_IN_ON   GPIO_WriteHigh(GPIOC, GPIO_PIN_4)
#define VALVE_IN_OFF  GPIO_WriteLow(GPIOC, GPIO_PIN_4)

#define PUMP_ON       GPIO_WriteHigh(GPIOD, GPIO_PIN_4)
#define PUMP_OFF      GPIO_WriteLow(GPIOD, GPIO_PIN_4)

#define VALVE_RO_IN_ON       GPIO_WriteHigh(GPIOB, GPIO_PIN_4)
#define VALVE_RO_IN_OFF      GPIO_WriteLow(GPIOB, GPIO_PIN_4)

#define VALVE_RO_OUT_ON       GPIO_WriteHigh(GPIOB, GPIO_PIN_5)
#define VALVE_RO_OUT_OFF      GPIO_WriteLow(GPIOB, GPIO_PIN_5)

#define InputPressure   GPIO_ReadInputPin(GPIOD, GPIO_PIN_2)
#define BucketPressure  GPIO_ReadInputPin(GPIOC, GPIO_PIN_3)
#define Water_Signal    GPIO_ReadInputPin(GPIOD, GPIO_PIN_1)

/* 时钟初始化 ----------------------------------------------------------------*/ 
void CLK_init(void)
{ 
  CLK_HSICmd(ENABLE);
  CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV8);  //内部高速时钟16MHz/8=2MHz
  CLK_SYSCLKConfig(CLK_PRESCALER_CPUDIV1);        //f_master=f_CPU=2MHz
  CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER1,ENABLE);   //使能TIM1时钟（2MHz）;
  CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER2,ENABLE);   //使能TIM2时钟（2MHz）;
  
  
  
  
  CLK_ClockSwitchCmd(DISABLE);                          //禁用时钟切换
}
/* 定时器初始化 ----------------------------------------------------------------*/ 
void TIM1_init(void)
{ 
  TIM1_TimeBaseInit(1000, TIM1_COUNTERMODE_UP, 1000, 1);  //TIM1计数时钟2MHz/1000;向上计数，计数周期1000，重复计数1（即每2次计数满产生溢出），溢出周期2*0.5=1s
  TIM1_ITConfig(TIM1_IT_UPDATE, ENABLE);      //开启定时器TIM1溢出中断；
}
void TIM2_init(void)
{
  TIM2_TimeBaseInit( TIM2_PRESCALER_1, 1985);   //TIM2使用2MHz时钟，2000计数周期溢出一次，即1ms定时，CPU执行时间修正15个计数周期
  TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE); //开启定时器TIM2溢出中断；
}

/* IO口初始化 ----------------------------------------------------------------*/ 
void _GPIO_(void)
{
  GPIO_Init(GPIOC, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_MODE_OUT_PP_LOW_FAST);
  GPIO_Init(GPIOD, GPIO_PIN_5 | GPIO_PIN_6, GPIO_MODE_OUT_PP_LOW_FAST);
  GPIO_Init(GPIOA, GPIO_PIN_3, GPIO_MODE_OUT_PP_LOW_FAST);
 
  GPIO_Init(GPIOD, GPIO_PIN_1 | GPIO_PIN_2, GPIO_MODE_IN_FL_NO_IT);     //PD1、2\PC3悬浮输入模式
  GPIO_Init(GPIOC, GPIO_PIN_3, GPIO_MODE_IN_FL_NO_IT);
 // GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_IN_FL_IT);        //PD2悬浮输入，带中断（默认为下降沿和低电平触发中断）  
  GPIO_Init(GPIOC, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_SLOW);      //PC4、PB4、PB5、PD4推挽输出，低速模式
  GPIO_Init(GPIOD, GPIO_PIN_4, GPIO_MODE_OUT_PP_LOW_SLOW); 
  GPIO_Init(GPIOB, GPIO_PIN_4 | GPIO_PIN_5, GPIO_MODE_OUT_PP_LOW_SLOW);   
  
  }

  void delay_Nsecond(uint16_t);
  void delay_N_Millisecond(uint16_t);
  void blink(uint16_t, uint16_t);
  void Making_Water(uint8_t);
  void Error_Handler(uint8_t);
  unsigned char ErrorFlag=0;
  unsigned int N_sec=0, N_Millisec=0;

int main(void)
{
  asm("sim"); 
  CLK_init();
  TIM1_init();
  TIM2_init();
  _GPIO_();
  asm("rim");
  //blink(2000,2);     //间隔2000ms，闪烁2次
 
  blink(500,3);
  blink(200,5);
  blink(100,10);
 
  while(1)
  {
    
   while(1)          //防止brake语句跳出循环系统复位
   {
    if(InputPressure)   //有水压时InputPressure为1，无水压时InputPressure为0
    {
       delay_N_Millisecond(200);
       if(InputPressure) 
         ErrorFlag &= ~0x80;      //进水有水压，则先清理停水标记 
       Error_Handler(ErrorFlag);  //刷新故障状态
       
	   if(InputPressure && Water_Signal && (BucketPressure == 0))   //有进水压力且桶空，启动制水
       {
          delay_Nsecond(23);          //桶低压信号在放水时出现，低压信号出现后延时23秒，以便把桶里的水放完，再开始制水
		  Making_Water(5);           //制水5分钟，根据通量来，要保证正常情况制水量能让桶低压开关闭合
       
          if(Water_Signal == 0)       //非正常满水，开始冲洗RO膜（停水了或者超时桶未满是不会清洗RO膜的）
            {
                VALVE_RO_OUT_ON;
				VALVE_IN_ON;
				delay_Nsecond(13);       //原水冲洗13秒
				VALVE_IN_OFF; 
				
                VALVE_RO_IN_ON;
                delay_Nsecond(15);      //冲洗15s
                VALVE_RO_IN_OFF;
                VALVE_RO_OUT_OFF;
                break;
            }
          
          if(ErrorFlag)  //如果制水过程中停水，则跳出本级循环 
             break;
          if(BucketPressure)          //正常制水5分钟，桶压力有了，表明储水正常，否则判定为漏水或者RO膜堵塞（水量太小）
          {
            if(Water_Signal)          //Water_Signal=1说明水压不够1.5kg，需要继续制水             
              Making_Water(25);        //继续制水25min
            if(ErrorFlag)              //如果制水过程中停水或者桶满水，则跳出本级循环
             break;  
			 
            if(Water_Signal == 0)       //正常满水，开始冲洗RO膜（停水了或者超时桶未满是不会清洗RO膜的）
            {
                VALVE_RO_OUT_ON;
				VALVE_IN_ON;
				delay_Nsecond(13);       //原水冲洗13秒
				VALVE_IN_OFF; 
				
                VALVE_RO_IN_ON;
                delay_Nsecond(15);      //冲洗15s
                VALVE_RO_IN_OFF;
                VALVE_RO_OUT_OFF;
				delay_Nsecond(3);       //等待电磁阀完全关闭，避免阀开期间检测到桶低压不正常
                break;
            }
            else
            { 
              ErrorFlag |= 0x02;         //制水超时
              Error_Handler(ErrorFlag);  //刷新故障状态             
            }
            
          }
          else 
          {                           //经过5min制水，桶压力没有回升
            ErrorFlag |= 0x01;        //可能是漏水或者RO膜堵塞（水量太小）
			break;
          }
       }
       else if((BucketPressure == 0) && (Water_Signal == 0))  //Water_Signal 桶水压大于2.5kg为0（断开），小于1.5kg为1（闭合）
              {												//高低压开关显示桶满，桶低压开关显示桶是空的
                ErrorFlag |= 0x08;                     
                Error_Handler(ErrorFlag);  //刷新故障状态
              } 
   }
    else 
    {
      delay_N_Millisecond(250);
      if(InputPressure == 0)            //停水告警，红灯常亮       
        ErrorFlag |= 0x80;              //标记停水    
    }   
    Error_Handler(ErrorFlag);
   
   }
  
  }
}   


void delay_Nsecond(uint16_t N)
{
   N_sec = N;
   TIM1->CR1 |= TIM1_CR1_CEN;   //启动TIM1计数
   while(N_sec > 0);  
   TIM1->CR1 &= (uint8_t)(~TIM1_CR1_CEN); //关闭TIM1计数 
}

void delay_N_Millisecond(uint16_t N)
{
   N_Millisec = N;
   TIM2->CR1 |= TIM1_CR1_CEN;   //启动TIM2计数
   while(N_Millisec > 0);  
   TIM2->CR1 &= (uint8_t)(~TIM1_CR1_CEN); //关闭TIM2计数 
}

/* 
输入参数：
Interval_ms：闪烁间隔时间（ms）；
Number     ：点亮次数，次数选择0时，常亮
*/
void blink(uint16_t Interval_ms, uint16_t Number)
{
	
 if(Number > 0)
   {
     for(;Number>0;Number--)
	   {
		   AlarmOn;
		   delay_N_Millisecond(Interval_ms);
	       AlarmOff;
		   delay_N_Millisecond(Interval_ms);
	   }	   
   }	 
 else                       //闪烁次数给0则常亮
    AlarmOn;
	
}

/*********************************************
输入参数：
WaitingTime：制水等待时间，单位分钟；
制水期间，每1s检查一次是否停水，发生停水则停止制水
**********************************************/
void Making_Water(uint8_t WaitingTime)
{
  uint16_t WaitingTime_second = WaitingTime * 60;
  VALVE_IN_ON;
  VALVE_RO_IN_OFF;
  VALVE_RO_OUT_OFF;
  delay_Nsecond(1);
  PUMP_ON;                                  //打开进水阀，关闭冲洗通路，启动水泵制水
  for(;WaitingTime_second>0;WaitingTime_second--)
  {
      if(ErrorFlag || (Water_Signal == 0))
        break;                           //有错误发生或者桶满了则停止制水，制水过程中停水也会产生错误，停止制水
      if(InputPressure)
        delay_Nsecond(1);
      else
      {
		delay_N_Millisecond(100);     //延迟100ms，二次确认是否停水
		if(InputPressure == 0)
		{							//停水处理措施：
			ErrorFlag |= 0x80;                //标记停水			
			PUMP_OFF;
			VALVE_RO_OUT_ON;              //如果停水管路有气体，入水低压开关可能误报停水，此时关闭水泵（防止真停水烧泵），打开RO排水阀，加速管路气体排除        
			delay_Nsecond(7);             //等待管路排气时间，根据需要调整，默认取质数7s
			VALVE_RO_OUT_OFF;
			if(InputPressure)                //水压恢复则继续制水
			{         
				PUMP_ON;
				ErrorFlag &= ~0x80;            //清零停水标记
			}
			else 
			{                              //真停水则关闭进水阀，长亮告警灯（前面已经刷新过停水标记了）
				VALVE_IN_OFF;  
				Error_Handler(ErrorFlag);    //刷新故障状态
				break;                     //停水时直接结束制水过程
			}   
			
		}	
        
      } 
  }
  PUMP_OFF;
  delay_Nsecond(3);         //水泵关闭缓冲时间
  VALVE_IN_OFF;             //不管是正常制水结束还是故障结束，关闭制水通路
  
}

/*********************************************
错误处理函数：
输入参数：；ErrorFlag
ErrorFlag------->bits: 7 6 5 4 3 2 1 0
bit 7 制水时发生停水/水压波动到0；

**********************************************/
void Error_Handler(uint8_t ErrorCode)
{
  switch(ErrorCode)
  {
    case 0x80:             //停水代码，此时告警灯常亮，来水后要软件清除错误代码
       AlarmOn;
       break;
    case 0x08:             //桶低压开关、高压开关均断开，桶空、桶满同时发生
       while(1)                  //告警灯短亮长灭告警，可能是高压开关故障开路且桶空、桶满时桶低压开关开路
              {
                AlarmOn;
                delay_N_Millisecond(100);
                AlarmOff;
                delay_N_Millisecond(1000);
              }
       break;
    case 0x88:
       while(1)                  //进水无水压，桶高低压开关均断开，此时告警长亮短灭
              {
                AlarmOn;
                delay_N_Millisecond(1000);
                AlarmOff;
                delay_N_Millisecond(100);
              }
       break;
    case 0x01:
       while(1)                  //制水10min，桶压力不升高，告警灯1s频率闪烁告警，需要检查输出通路
            {
              AlarmOn;
              delay_Nsecond(1);
              AlarmOff;
              delay_Nsecond(1);
            }
    case 0x02:
       while(1)                   //制水超时，告警灯0.2s频率快闪告警，，可能膜的通量锐减，制水量显著变小
              {
                AlarmOn;
                delay_N_Millisecond(200);
                AlarmOff;
                delay_N_Millisecond(200);
              }
     default: AlarmOff;
  }
}






#pragma vector=0x0D     //TIM1_OVR_UIF_vector         //TIM1溢出中断号
__interrupt void TIM1_OVR_UIF_IRQHandler(void)
{
  TIM1->SR1 &= ~0x01;    //清除中断标记位
  N_sec--;
}

#pragma vector=0x0F     //TIM2_OVR_UIF_vector         //TIM2溢出中断号
__interrupt void TIM2_OVR_UIF_IRQHandler(void)
{
  TIM2->SR1 &= ~0x01;    //清除中断标记位
  N_Millisec--;
}


#ifdef USE_FULL_ASSERT
void assert_failed(u8* file, u32 line)
{
/* User can add his own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
/* Infinite loop */
while (1)
{
}
}
#endif


/******************* (C) COPYRIGHT 2022  *****END OF FILE****/