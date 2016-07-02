
#define __DRIVER_VISION_GLOBALS

#include "Config.h"
#include "Driver_vision.h"
#include "Driver_CloudMotor.h"
#include <math.h>

/*
X正 右
Y正 下
Z正 前
*/


/**
  * @brief  视觉初始化
  * @param  void
  * @retval void
  */
void Vision_InitConfig(void)
{
    PCDataBufferPoint = 0;
    EnemyDataBufferPoint = 0;
    
    EnemyDataBuffer[EnemyDataBufferPoint].ID = 1;
    EnemyDataBuffer[EnemyDataBufferPoint].Time = 0;
    EnemyDataBuffer[EnemyDataBufferPoint].X = 0;
    EnemyDataBuffer[EnemyDataBufferPoint].Y = 0;
    EnemyDataBuffer[EnemyDataBufferPoint].Z = 1;
}


/**
  * @brief  将直角坐标系转换为电机编码器角度
  * @param  X
  * @param  Y
  * @param  Z
  * @param  0 不考虑重力加速度      1 考虑重力加速度
  * @retval 角度(编码器，中间为0）
  */
AngleI_Struct RecToPolar(float X, float Y, float Z, uint8_t mode)
{
    AngleI_Struct ReturnData;
    float Distance = sqrt(X * X + Z * Z);
    float distance, radian;
    
    ReturnData.H =  - atan(X / Z) * 1303.7973F;
    
    distance = sqrt(X * X + Z * Z);
    if(mode == 0)
    {
    //不考虑重力加速度
        ReturnData.V = -atan(Y / distance) * 1303.7973F;
    }
    else
    {
    //考虑重力加速度
        radian = (atan(((AFG * distance * distance) / (GUNSpeed * GUNSpeed) - Y) / sqrt(Y * Y + distance * distance)) - atan(Y / distance)) / 2;
        ReturnData.V = radian * 1303.7973F;
    }
    
    return ReturnData;
}


/**
  * @brief  路径拟合核心函数，根据输入进行一次拟合
  * @param  预判样本时间长度
  * @param  预判时间
  * @param  预判位置
  * @retval 0 成功        1 失败
  */
uint8_t ForcastCore(uint16_t SampleTime, uint16_t ForcastTime, Point_Struct *ForcastPoint)
{
    int RelativeTime;       //相对时间，防止绝对时间超范围
    uint16_t index = 0, Currentindex;
    uint16_t SampleNum = 0;
    

    float A = 0;
    float B = 0;
    float C = 0;
    float D = 0;
    float E = 0;
    
    float Fx = 0;
    float Gx = 0;
    float Hx = 0;
    float Ix = 0;
    float Jx = 0;
    float Kx = 0;
    float Lx = 0;
    float Mx = 0;
    float Nx = 0;
    
    float Fz = 0;
    float Gz = 0;
    float Hz = 0;
    float Iz = 0;
    float Jz = 0;
    float Kz = 0;
    float Lz = 0;
    float Mz = 0;
    float Nz = 0;
    
    float Pax, Pbx, Pcx;
    float Paz, Pbz, Pcz;
    
    //寻找起点
    for(SampleNum = 0; SampleNum < ENEMYDATABUFFERLENGHT; SampleNum++)
    {
        if(EnemyDataBuffer[(EnemyDataBufferPoint + ENEMYDATABUFFERLENGHT - SampleNum - 1) % ENEMYDATABUFFERLENGHT].Time + SampleTime < EnemyDataBuffer[EnemyDataBufferPoint].Time)
        {
            break;
        }
    }
    
    //拟合数据量过少
    if(SampleNum < 5)
    {
        return 1;
    }
    
    E =  -(1 + SampleNum);
    
    //数据拟合
    for(index = 0; index <= SampleNum; index++)
    {
        Currentindex = (EnemyDataBufferPoint + ENEMYDATABUFFERLENGHT - index) % ENEMYDATABUFFERLENGHT;
        
        RelativeTime = EnemyDataBuffer[Currentindex].Time - EnemyDataBuffer[(EnemyDataBufferPoint + ENEMYDATABUFFERLENGHT - SampleNum) % ENEMYDATABUFFERLENGHT].Time;        

//        Relative2 = RelativeTime * RelativeTime;
//        Relative3 = Relative2 * RelativeTime;
//        Relative4 = Relative3 * RelativeTime;
        
        A = A - RelativeTime * RelativeTime * RelativeTime * RelativeTime;
        B = B - RelativeTime * RelativeTime * RelativeTime;
        C = C - RelativeTime * RelativeTime;
        D = D - RelativeTime;
        
        Fx = Fx + RelativeTime * RelativeTime * EnemyDataBuffer[Currentindex].X;
        Gx = Gx + RelativeTime * EnemyDataBuffer[Currentindex].X;
        Hx = Hx + EnemyDataBuffer[Currentindex].X;
        
        Fz = Fz + RelativeTime * RelativeTime * EnemyDataBuffer[Currentindex].Z;
        Gz = Gz + RelativeTime * EnemyDataBuffer[Currentindex].Z;
        Hz = Hz + EnemyDataBuffer[Currentindex].Z;
    }
    

    Ix = D * Fx - C * Gx;
    Jx = A * D - B * C;
    Kx = B * D - C * C;
    Lx = E * Fx - Hx * C;
    Mx = A * E - C * C;
    Nx = B * E - C * D;
    
    Iz = D * Fz - C * Gz;
    Jz = A * D - B * C;
    Kz = B * D - C * C;
    Lz = E * Fz - Hz * C;
    Mz = A * E - C * C;
    Nz = B * E - C * D;

    Pax = (Ix * Nx - Lx * Kx) / (Mx * Kx - Jx * Nx);
    Pbx = -(Ix + Pax * Jx)  / Kx;
    Pcx = - (Fx + Pax * A + Pbx * B) / C;
    
    Paz = (Iz * Nz - Lz * Kz) / (Mz * Kz - Jz * Nz);
    Pbz = -(Iz + Paz * Jz) / Kz;
    Pcz = - (Fz + Paz * A + Pbz * B) / C;
    
    ForcastTime += EnemyDataBuffer[EnemyDataBufferPoint].Time - EnemyDataBuffer[(EnemyDataBufferPoint + ENEMYDATABUFFERLENGHT - SampleNum) % ENEMYDATABUFFERLENGHT].Time;
    
    ForcastPoint->X = (ForcastTime * ForcastTime * Pax + Pbx * ForcastTime + Pcx);
    ForcastPoint->Y = EnemyDataBuffer[Currentindex].Y;
    ForcastPoint->Z = (Paz * ForcastTime * ForcastTime + Pbz * ForcastTime + Pcz);
    
    return 0;
}


/**
  * @brief  进行一次拟合，输出目标角度（相对云台电机）
  * @param  拟合样本时间(ms)
  * @param  拟合时间长度(ms)
  * @param  时间模式    0 自定义时间（即ForcastTime）， 1 自动设定预判时间（根据距离及子弹速度确定）
  * @retval 0 二次拟合成功        1 因样本数据不够，拟合失败直接使用当前位置作为目标位置
  */
uint8_t ForcastOnce(uint16_t SampleTime, uint16_t ForcastTime, AngleI_Struct *ForcastAngle, uint8_t TimeMode)
{
    Point_Struct ForcastPoint;
    float distance;
    
    //自动调节拟合时间长度
    if(TimeMode)
    {
        distance = sqrt(EnemyDataBuffer[EnemyDataBufferPoint].X * EnemyDataBuffer[EnemyDataBufferPoint].X + EnemyDataBuffer[EnemyDataBufferPoint].Y * EnemyDataBuffer[EnemyDataBufferPoint].Y);
        ForcastTime = distance * 1000 / GUNSpeed + ProgonsisCompensate;
    }
    
    if(ForcastCore(SampleTime, ForcastTime, &ForcastPoint) == 0)        //二次拟合成功
    {
        //目标点转换为目标角度
        *ForcastAngle = RecToPolar(ForcastPoint.X, 
                                    ForcastPoint.Y, 
                                    ForcastPoint.Z,
                                    1);
        
        return 0;
    }
    //二次拟合失败
    else
    {
        //拟合失败直接使用当前位置作为目标位置
        *ForcastAngle = RecToPolar(EnemyDataBuffer[EnemyDataBufferPoint].X, 
                                    EnemyDataBuffer[EnemyDataBufferPoint].Y, 
                                    EnemyDataBuffer[EnemyDataBufferPoint].Z,
                                    1);
        
        return 1;
    }
}


/**
  * @brief  射击评估
  * @param  当前时间
  * @retval 0 评估结果不适合           1 评估结果适合发射
  */
uint8_t VShot_Evaluation(portTickType CurrentTick)
{
    
/**********************     傻逼评估方式      **********************/
    //时间误差小于±10
//    if((CurrentTick + 10 > ForcastTarget.TargetTick) && (CurrentTick - 10 < ForcastTarget.TargetTick))
//    {
        //角度误差小于50线
        if((CloudParam.Yaw.RealEncoderAngle - ForcastTarget.Target.H - YawCenter > -50) && (CloudParam.Yaw.RealEncoderAngle - ForcastTarget.Target.H  - YawCenter < 50))
        {
            return 1;
        }
        else
        {
            return 0;
        }
//    }
//    else
//    {
//    return 0;
//    }
        
        
/**********************     评估方式二       **********************/
    
    
    
    
    
    
}




