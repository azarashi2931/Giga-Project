#include <FlexiTimer2.h>                          //ラインセンサのタイマ割り込み用
#include "src/utilities/KDMacroUtilities.hpp"     //共通マクロ
#include "src/utilities/KDDataStructure.hpp"      //共通データ構造
#include "src/hardware/KDHardwere.hpp"            //ハードウェア依存定数の保存
#include "src/ir/KDIRSensor.hpp"                  //IRセンサ制御クラス
#include "src/ultrasonic/KDUltraSonicSensor.hpp"  //
#include "src/ultrasonic/KDUltraSonicSensors.hpp" //超音波センサ制御クラス、超音波センサ使用不能
#include "src/kicker/KDKicker.hpp"                //キッカー制御クラス
#include "src/line/KDLineSensor.hpp"              //ライン読み取りクラス
#include "src/line/KDLineSensors.hpp"             //ライン読み取りクラス
#include "src/gyro/KDGyroSensor.hpp"              //ジャイロ読み取りクラス
#include "src/catch/KDCatchSensor.hpp"            //捕捉センサ読み取りクラス
#include "src/ui/KDUIUnitCommunication.hpp"       //UIユニットとの通信用クラス
#include "src/pid/KDPIDControl.hpp"               //pid制御クラス、I,D未実装
#include "src/move/KDMotor.hpp"                   //モータ単体制御クラス
#include "src/move/KDThreeMotors.hpp"             //三輪モータ制御
#include "src/move/KDMoveLocker.hpp"              //移動ロック用クラス
#include "src/move/KDMoveCtrl.hpp"                //移動制御クラス
#include "src/utilities/KDDebugUtility.hpp"       //デバック用クラス
#include "src/thread/KDInterThreadData.hpp"       //スレッド間通信用クラス
#include "src/ui/switch/KDSwitchObserver.hpp"     //スイッチの監視クラス
#include "src/sound/TonePlayer.hpp"               //スピーカーによる音楽再生クラス
#include "src/sound/Pitches.hpp"

static constexpr int16_t DefaultPower = 256;

//センサ制御クラスのインスタンス
KDIRSensor irSensors(&Serial4);
KDKicker<KDHardwere::KickerPin> kicker;
KDGyroSensor gyroSensor(&Serial5);
KDCatchSensor catchSensor(KDHardwere::CatchSensorPin);

KDUltraSonicSensor frontUSSensor(KDHardwere::FrontUSSensorPin, 0.3);
KDUltraSonicSensor rearUSSensor(KDHardwere::RearUSSensorPin, 0.3);
KDUltraSonicSensor rightUSSensor(KDHardwere::RightUSSensorPin, 0.3);
KDUltraSonicSensor leftUSSensor(KDHardwere::LeftUSSensorPin, 0.3);
KDUltraSonicSensors usSensors = KDUltraSonicSensors(&frontUSSensor, &rearUSSensor, &rightUSSensor, &leftUSSensor);

typedef KDLineSensor<KDHardwere::LineAnalogPin> FrontLineSensor; //FrontLineSensorPinが動かないのでanalogPin
typedef KDLineSensor<KDHardwere::RearLineSensorPin> RearLineSensor;
typedef KDLineSensor<KDHardwere::RightLineSensorPin> RightLineSensor;
typedef KDLineSensor<KDHardwere::LeftLineSensorPin> LeftLineSensor;
typedef KDLineSensors<FrontLineSensor, RearLineSensor, RightLineSensor, LeftLineSensor> LineSensors;
FrontLineSensor frontLineSensor;
RearLineSensor rearLineSensor;
RightLineSensor rightLineSensor;
LeftLineSensor leftLineSensor;
LineSensors lineSensors(&frontLineSensor, &rearLineSensor, &rightLineSensor, &leftLineSensor, KDHardwere::LineAnalogPin, KDHardwere::LineThreshold);

KDUIUnitCommunication uiUnitCommunication(&Serial2);
int melody[1] = {Pitches::NoteC4};
int noteDurations[1] = {4};
TonePlayer tonePlayer(1, melody, noteDurations);
void onMainSwitch()
{
    //setGreen
    //setline
    tonePlayer.play();
}
void onSubSwitch()
{
    //setWhite
}
KDSwitchObserver<KDHardwere::Switch1Pin> mainSwitchObserver(onMainSwitch);
KDSwitchObserver<KDHardwere::Switch2Pin> subSwitchObserver(onSubSwitch);

//方位補正用のPID制御インスタンスだが、P制御しか実装してない
//KP=0.071~0.072の範囲
//KI=KP/DELTA_TIME
//KD＝KP*DELTA_TIME
//static constexpr float GyroPidKP = 0.072;
static constexpr float GyroPidKP = -0.08;
static constexpr float GyroPidKI = -GyroPidKP / 14.5;
static constexpr float GyroPidKD = -GyroPidKP * 14.5;
KDPIDControl pidControl(GyroPidKP, GyroPidKI, GyroPidKD, -7.0f, 7.0f);

//スレッド間のデータ共有用インスタンス
KDInterThreadData interThreadData = KDInterThreadData();

//スタート後かのフラグ、スタートダッシュ用、別の方法に変えたい
bool isStarted = false;

typedef KDMotor<KDHardwere::RightMotorDirectionPin, KDHardwere::RightMotorPwmPin, true> RightMotor;
typedef KDMotor<KDHardwere::LeftMotorDirectionPin, KDHardwere::LeftMotorPwmPin, true> LeftMotor;
typedef KDMotor<KDHardwere::RearMotorDirectionPin, KDHardwere::RearMotorPwmPin, false> RearMotor;
typedef KDThreeMotors<RightMotor, LeftMotor, RearMotor> ThreeMotors;
RightMotor rightMotor(1.0);
LeftMotor leftMotor(1.0);
RearMotor rearMotor(1.0);
ThreeMotors threeMotors(&rightMotor, &leftMotor, &rearMotor);
KDMoveLocker moveLocker;
KDMoveCtrl<ThreeMotors> moveCtrl(&threeMotors, &moveLocker);

void setup()
{
    //シリアル通信及びI2C通信の初期化
    //ピンモードの設定など
    KDHardwere::initialize();

    //初期化が必要なモジュールの初期化
    gyroSensor.init();
    kicker.init();
    irSensors.init();

    digitalWriteFast(KDHardwere::LEDPin, HIGH);

    //analogWrite(KDHardwere::LineThreshold, 85);
    analogWrite(KDHardwere::LineThreshold, 400);

    //ライン処理のためのタイマ割り込みを設定して開始
    FlexiTimer2::set(1, 1.0 / 7500, lineProcess1);
    FlexiTimer2::start();
}

void loop()
{
    //スイッチのテスト
    //mainSwitchObserver.printValue();
    //KDDebugUtility::printValueWithTag("switch", uiUnitCommunication.read());

    //スイッチがオフならこのif分以降は実行されない
    if (!mainSwitchObserver.read())
    {
        moveCtrl.moveByLocalDegreeAndPower(0, 0, 0);
        Serial.println("switch off");
        delay(10);
        return;
    }

    //スタートダッシュスイッチ
    if (!isStarted)
    {
        //if (subSwitchObserver.read())
        {
            isStarted = true;
            moveCtrl.moveByLocalDegreeAndPower(0, DefaultPower, 0);
        }
    }

    //------------テスト用コード------------
    //IRSenosr用テストコード
    //irSensors.printValue();

    //GyroSensorのテスト用コード
    /*if (Serial5.available())
        Serial.println();
    else
    {
        Serial.println();
        Serial.print("none");
    }
    while (Serial5.available() > 0)
    {
        byte data = Serial5.read();
        Serial.print("data : ");
        Serial.print(data, HEX);
        Serial.println();
    }*/
    //gyroSensor.printValue();

    //UltraSonicSensorのテスト用コード
    //usSensors.printValue();

    //LineSensorのテスト用コード
    //lineSensors.printValue();

    //LineProcess()
    //delay(10);

    //キッカーテスト
    /*FlexiTimer2::stop();
    static unsigned long lastKick = 0;
    if (millis() - lastKick > 3000)
    {
        kicker.kickWithSerialPrint();
        lastKick = millis();
    }
    FlexiTimer2::stop();*/

    //捕捉センサテストコード
    //catchSensor.printValue();

    //モータテスト用コード
    /*KDThreeMotors::SetMotorPower(128, 0, 0);
    delay(1000);
    KDThreeMotors::SetMotorPower(0, 128, 0);
    delay(1000);
    KDThreeMotors::SetMotorPower(0, 0, 128);
    delay(1000);*/
    //KDThreeMotors::SetMotorPower(256, 256, 256);
    /*KDDebugUtility::printValueWithTag("right", analogRead(KDHardwere::RightMotorElectricCurrentMonitoringPin));
    KDDebugUtility::printValueWithTag("left", analogRead(KDHardwere::RightMotorElectricCurrentMonitoringPin));
    KDDebugUtility::printValueWithTag("rear", analogRead(KDHardwere::RightMotorElectricCurrentMonitoringPin));
    KDDebugUtility::println();*/

    //簡易モータテストコード
    //moveCtrl.moveByLocalDegreeAndPower(0, DefaultPower, 0);

    //delay(10);
    //return;
    //------------------------------------

    /*static unsigned long lastWaitMs = 0;
    if(millis() - lastWaitMs < 10) return;
    lastWaitMs = millis();*/

    //センサから数値を取得-----------------------------
    //機体の角度を取得
    int bodyDirection = gyroSensor.read();
    if (bodyDirection > 180) //これとか入れる
        bodyDirection -= 360;

    //ボールセンサの値を取得
    BallStatus_t *ballStatus = irSensors.read();
    VectorDD_t ballLocalVector = ballStatus->polarVector;
    constexpr bool isUseBaseVector = false;
    if (isUseBaseVector)
    {
        static VectorXY_t baseVector = {0, 0};
        constexpr float filteringRate = 0.05;
        constexpr int scalingRate = 30;
        baseVector.x = round(ballStatus->deltaRectangularVector.x * scalingRate * filteringRate + baseVector.x * (1 - filteringRate));
        baseVector.y = round(ballStatus->deltaRectangularVector.y * scalingRate * filteringRate + baseVector.y * (1 - filteringRate));
        moveCtrl.baseVector = baseVector;
        KDDebugUtility::printValueWithTag("dx", moveCtrl.baseVector.x);
        KDDebugUtility::printValueWithTag("dy", moveCtrl.baseVector.y);
        KDDebugUtility::println();
    }

    //捕捉時には50~170程度。250未満で捕捉判定なら安全のはず
    bool isCatching = analogRead(KDHardwere::CatchSensorPin) > 300;
    //----------------------------------------------

    int targetAttitude = 0;   //目標の姿勢
    int targetDegrees = 0;    //目標の方向
    int power = DefaultPower; //defaultPower
    int angularVelocity = 0;  //出力する角速度

    static unsigned long previousMillis = 0;
    unsigned long deltaMillis = millis() - previousMillis;

    //ボールの角度を取り出し
    int ballDegrees = ballLocalVector.degrees;

    if (bodyDirection > 45 || bodyDirection < -45)
    {
        power = 0;
    }
    else
    {
        //ボールを捕捉している場合
        static unsigned long catichingMillis = 0;
        if (isCatching && ballLocalVector.distance < 35 && ballDegrees < 10 && ballDegrees > -10)
        {
            catichingMillis += deltaMillis;
            //KDDebugUtility::printValueWithTag("ms", catichingMillis);
            //KDDebugUtility::println();

            power = round(power * 1.5f);
            int rightDistance = usSensors.getRightDistance();
            int leftDistance = usSensors.getLeftDistance();
            int diff = rightDistance - leftDistance;
            if (true /*abs(diff) < 20*/)
            {
                targetDegrees = ballDegrees * 0.5;
                if (catichingMillis > 100)
                {
                    //kicker.kick();
                    catichingMillis = 0;
                }
            }
            else
                targetDegrees = constrain(diff * 0.5, -15, 15);
        }
        //ボールを捕捉していないが視認している場合
        else if (ballLocalVector.distance < 400 && ballLocalVector.distance != 0)
        {
            const int absBallDegree = abs(ballDegrees);
            const bool isNear = ballLocalVector.distance < 30;
            const bool isMiddleNear = ballLocalVector.distance < 50;
            const bool isBallOnFront = absBallDegree < 35;
            const bool isBallOnSideFront = absBallDegree >= 35 && absBallDegree < 90;
            const bool isBallOnSideBack = absBallDegree >= 90 && absBallDegree < 135;
            const bool isBallOnBack = absBallDegree >= 135;
            //KDDebugUtility::printValueWithTag("near", isNear);
            //KDDebugUtility::printValueWithTag("midnear", ballLocalVector.distance);

            //targetDegrees = getSimpleTargetDegrees(ballDegrees, ballLocalVector.distance);
            targetDegrees = getCalcTargetDegrees(ballDegrees, ballLocalVector.distance);
            /*if (isUseBaseVector)
            {
                if (isNear && isBallOnSideFront)
                    targetDegrees = ballDegrees * 3;
                else if (isNear)
                    targetDegrees = ballDegrees * 1.9;
                else if (isMiddleNear && isBallOnSideFront)
                    targetDegrees = ballDegrees * 1.8;
                else if (isMiddleNear && isBallOnSideBack)
                    targetDegrees = ballDegrees * 1.9;
                else if (isBallOnBack)
                    targetDegrees = ballDegrees * 1.5;
                else
                    targetDegrees = ballDegrees * 1.3;
            }
            else
            {
                targetDegrees = getNonSmoothTargetDegrees();
            }*/

            //targetDegrees = getDeffensiveTargetDegrees(ballDegrees,ballLocalVector.distance);
        }
        //視認していない場合
        else
        {
            //超音波使えればコートの中心へ？
            targetDegrees = 180;
            power = 100;
        }
    }

    //ボールの後ろにい続けるやつ
    //power = DefaultPower;
    //targetDegrees = ballDegrees > 0 ? 90 : -90;

    angularVelocity = getAngularVelocity(bodyDirection, targetAttitude);

    lockMovementByLineVector();

    //モータに出力
    moveCtrl.moveByLocalDegreeAndPower(targetDegrees, power, angularVelocity, isUseBaseVector);
    previousMillis = millis();
    delay(10);
}

INLINE int getAngularVelocity(int bodyDirection, int targetAttitude)
{
    float floatAngularVelocity = pidControl.Calculate(bodyDirection, targetAttitude);
    return round(floatAngularVelocity);
}

INLINE void lockMovementByLineVector()
{
    /*VectorXY_t lineVector = {0, 0};
    unsigned long timeStamp = 0;
    interThreadData.read(&lineVector, &timeStamp);
    if (millis() - timeStamp < 5000)
    {
        //moveLocker.lockMovement((char)lineVector.x, (char)lineVector.y);
    }
    else*/
    {
        moveLocker.unlockMovement();
    }
}

//タイマ割り込みで呼ばれるライン処理用関数群
//--------------------------------------
void lineProcess1()
{
    VectorXY_t reactedLineDirecitionVector = {0, 0}; //反応したラインセンサの保存
    getReactedLineVector(&reactedLineDirecitionVector);
    if (reactedLineDirecitionVector.x == 0 && reactedLineDirecitionVector.y == 0)
    {
        //return;
        if (lineSensors.checkRearLineSensor() && !lineSensors.checkLeftLineSensor())
            reactedLineDirecitionVector.y = -1;
        else if (!lineSensors.checkRearLineSensor() && lineSensors.checkLeftLineSensor())
            reactedLineDirecitionVector.x = -1;
        else
        {
            reactedLineDirecitionVector.x = -1;
            reactedLineDirecitionVector.y = -1;
        }
    }
    interThreadData.push(reactedLineDirecitionVector);

    int count = 0;
    while (mainSwitchObserver.read() &&           //メインスイッチの確認
           (lineSensors.checkFrontLineSensor() || //---------------------------------
            lineSensors.checkRearLineSensor() ||  //ラインセンサが一つでも反応しているか
            lineSensors.checkRightLineSensor() || //---------------------------------
            lineSensors.checkLeftLineSensor()) &&
           !(reactedLineDirecitionVector.x == 0 && reactedLineDirecitionVector.y == 0) &&
           count < 100) //---------------------------------
    {
        for (int i = 0; i < 10 && !(reactedLineDirecitionVector.x == 0 && reactedLineDirecitionVector.y == 0); i++)
            moveCtrl.moveByLocalVector2(-(int)reactedLineDirecitionVector.x * DefaultPower, -(int)reactedLineDirecitionVector.y * DefaultPower, 0);

        count++;
    }

    //if (!digitalReadFast(KDHardwere::Switch1Pin))
    //    moveCtrl.moveByLocalVector2(0, 0, 0);

    reactedLineDirecitionVector = {0, 0};
}

void getReactedLineVector(VectorXY_t *vector)
{
    vector->x = 0;
    vector->y = 0;
    if (lineSensors.checkFrontLineSensor())
        vector->y++;
    if (lineSensors.checkRearLineSensor())
        vector->y--;
    if (lineSensors.checkRightLineSensor())
        vector->x++;
    if (lineSensors.checkLeftLineSensor())
        vector->x--;
}

void lineProcess2()
{
    VectorXY_t reactedLineDirecitionVector = {0, 0}; //反応したラインセンサの保存
    int count = 0;
    while (mainSwitchObserver.read() &&           //メインスイッチの確認
           (lineSensors.checkFrontLineSensor() || //---------------------------------
            lineSensors.checkRearLineSensor() ||  //ラインセンサが一つでも反応しているか
            lineSensors.checkRightLineSensor() || //---------------------------------
            lineSensors.checkLeftLineSensor()) &&
           count < 100) //---------------------------------
    {
        const bool front = lineSensors.checkFrontLineSensor();
        const bool rear = lineSensors.checkRearLineSensor();
        const bool right = lineSensors.checkRightLineSensor();
        const bool left = lineSensors.checkLeftLineSensor();

        if (reactedLineDirecitionVector.x == 0 &&
            reactedLineDirecitionVector.y == 0)
        {
            reactedLineDirecitionVector = {0, 0};
            if (front)
                reactedLineDirecitionVector.y++;
            if (rear)
                reactedLineDirecitionVector.y--;
            if (right)
                reactedLineDirecitionVector.x++;
            if (left)
                reactedLineDirecitionVector.x--;
            /*if (right && left)
                reactedLineDirecitionVector.x += 2;
            if (front && rear)
                reactedLineDirecitionVector.y -= 2;*/
            if (reactedLineDirecitionVector.x == 0 && reactedLineDirecitionVector.y == 0)
                return;

            interThreadData.push(reactedLineDirecitionVector);
        }

        //方位の調整-----------------------
        int angularVelocity = 0;
        //-------------------------------

        for (int i = 0; i < 10; i++)
            moveCtrl.moveByLocalVector2(-(int)reactedLineDirecitionVector.x * DefaultPower, -(int)reactedLineDirecitionVector.y * DefaultPower, angularVelocity);

        count++;
    }

    //if (!digitalReadFast(KDHardwere::Switch1Pin))
    //    moveCtrl.moveByLocalVector2(0, 0, 0);

    reactedLineDirecitionVector = {0, 0};
}

int getSimpleTargetDegrees(int ballDegrees, int distance)
{
    const int absBallDegree = abs(ballDegrees);
    const bool isNear = distance < 30;
    const bool isMiddleNear = distance < 50;
    const bool isBallOnFront = absBallDegree < 35;
    const bool isBallOnSideFront = absBallDegree >= 35 && absBallDegree < 90;
    const bool isBallOnSideBack = absBallDegree >= 90 && absBallDegree < 135;
    const bool isBallOnBack = absBallDegree >= 135;
    if (isNear)
        return ballDegrees * 2.1;
    else if (isMiddleNear)
        return ballDegrees * 1.5;

    return ballDegrees * 1.3;
}

int getCalcTargetDegrees(int ballDegrees, int distance)
{
    const int absBallDegree = abs(ballDegrees);
    const bool isNear = distance < 30;
    const bool isMiddleNear = distance < 50;
    const bool isMiddleFar = distance < 70;
    const bool isBallOnFront = absBallDegree < 35;
    const bool isBallOnSide = absBallDegree >= 35 && absBallDegree < 135;
    const bool isBallOnSideBack = absBallDegree >= 135 && absBallDegree < 160;
    const bool isBallOnBack = absBallDegree >= 160;
    if (isMiddleNear && isBallOnBack)
        return ballDegrees * 3;
    else if (isMiddleNear && isBallOnSideBack)
        return ballDegrees * 4;
    else if (isMiddleNear && isBallOnSide)
        return ballDegrees * 3.5;
    else if (isMiddleNear)
        return ballDegrees * 2.1;
    else
        return ballDegrees * 1.5;

    return ballDegrees * 1.3;
}

int getNonSmoothTargetDegrees(int ballDegrees, int distance)
{
    const int absBallDegree = abs(ballDegrees);
    const bool isNear = distance < 30;
    const bool isMiddleNear = distance < 50;
    const bool isBallOnFront = absBallDegree < 35;
    const bool isBallOnSideFront = absBallDegree >= 35 && absBallDegree < 90;
    const bool isBallOnSideBack = absBallDegree >= 90 && absBallDegree < 135;
    const bool isBallOnBack = absBallDegree >= 135;
    if (isNear && isBallOnSideFront)
        return 180 - KDMath::sign(ballDegrees);
    else if (isNear)
        return ballDegrees * 3.5;
    else if (isMiddleNear && isBallOnSideFront)
        return 180 - KDMath::sign(ballDegrees);
    //targetDegrees = ballDegrees * 2;
    else if (isMiddleNear && isBallOnSideBack)
        return ballDegrees * 2;
    else if (isBallOnBack)
        return ballDegrees * 1.7;

    return ballDegrees * 1.3;
}

int getDeffensiveTargetDegrees(int ballDegrees, int distance)
{
    const int absBallDegree = abs(ballDegrees);
    const bool isNear = distance < 30;
    const bool isMiddleNear = distance < 50;
    const bool isBallOnFront = absBallDegree < 35;
    const bool isBallOnSideFront = absBallDegree >= 35 && absBallDegree < 90;
    const bool isBallOnSideBack = absBallDegree >= 90 && absBallDegree < 135;
    const bool isBallOnBack = absBallDegree >= 135;
    if (isBallOnFront || isBallOnSideFront)
        return 90 * KDMath::sign(ballDegrees);

    if (isBallOnBack)
        return 90 * KDMath::sign(ballDegrees) * -1;

    return 180;
}
