//
// Created by Administrator on 2021/12/12.
//

#include <SLES/OpenSLES.h>
#include <stdlib.h>

#include <android/log.h>
#include <SLES/OpenSLES_Android.h>
#include <string.h>
#include <tgmath.h>

#define LOG_TAG "native-player"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG ,__VA_ARGS__) // 定义LOGE类型
#define RECORDER_SIZE 128

SLObjectItf engineObject;
SLEngineItf audioEngine;
SLRecordItf recorderRecorder;
SLAndroidSimpleBufferQueueItf recorderBufferQueue; //Buffer接口
SLDataFormat_PCM slDataFormatPcm;

uint8_t recordBuffer[RECORDER_SIZE];
FILE * pcm_file;
long frame_size;
long file_size=0;
unsigned long recorder_ms=0;
unsigned  char play_status=0;//0 start 1 recording 2 pause 3 stop
long wave_average=0;
JavaVM* javaVm;
jmethodID progressListenerId;
jmethodID waveListenerId;
jobject progressListenerObject;
int dbArray[8];
int dbArrayCount=0;


/**
 * 获取所有振幅之平均值 计算db (振幅最大值 2^16-1 = 65535 最大值是 96.32db)
 * 16 bit == 2字节 == short int
 * 无符号16bit：96.32=20*lg(65535);
 *
 * @param pcmdata 转换成char类型，才可以按字节操作
 * @param size pcmdata的大小
 * @return
 */
//int getPcmDB(const unsigned char *pcmdata, size_t size,int bitFormat) {
//
//    int db = 0;
//    short int value = 0;
//    double sum = 0;
//    for(int i = 0; i < size; i += bitFormat/8)
//    {
//        memcpy(&value, pcmdata+i, bitFormat/8); //获取2个字节的大小（值）
//        sum += abs(value); //绝对值求和
//    }
//    sum = sum / (size / (bitFormat/8)); //求平均值（2个字节表示一个振幅，所以振幅个数为：size/2个）
//    if(sum > 0)
//    {
//        db = (int)(20.0*log10(sum));
//    }
//    return db;
//}

int getPcmDB16(const unsigned char *pcmdata, size_t size,int bit_format) {
    int db = 0;
    short int value = 0;
    double sum = 0;
    for(int i = 0; i < size; i += bit_format/8)
    {
        memcpy(&value, pcmdata+i, bit_format/8); //获取2个字节的大小（值）
        sum += abs(value); //绝对值求和
    }
    sum = sum / (size / (bit_format/8)); //求平均值（2个字节表示一个振幅，所以振幅个数为：size/2个）
    if(sum > 0)
    {
        db = (int)(20.0*log10(sum));
    }
    return db;
}


unsigned char dbNotify=0;
static void RecordCallback(SLAndroidSimpleBufferQueueItf bufferQueue, void *context) {
    if (NULL==recorderRecorder)
        return;

    //fwrite(recordBuffer, 1, RECORDER_SIZE, pcm_file);
    int db=getPcmDB16(recordBuffer,RECORDER_SIZE,16);
    dbArray[dbArrayCount]=db;
    dbArrayCount++;
    dbNotify=0;
    if (dbArrayCount>=8){
        int sum=0;
        for (int i = 0; i < dbArrayCount; ++i) {
            sum+=dbArray[i];
            dbNotify=1;
        }
        db=sum/dbArrayCount;
        dbArrayCount=0;
    }


    frame_size+=(RECORDER_SIZE/((slDataFormatPcm.bitsPerSample/8)*slDataFormatPcm.numChannels));
    file_size+=RECORDER_SIZE;
    if (play_status==3) {

//        fclose(pcm_file);
        JNIEnv* env= NULL;

        (*javaVm)->AttachCurrentThread(javaVm,&env,NULL);

        int result=(*javaVm)->GetEnv((javaVm),(void **)&env,JNI_VERSION_1_4);

        if (result==JNI_OK){
            //LOGE("frame_size %lu recorder_ms%lu",frame_size,recorder_ms);
            if (dbNotify)
                (*env)->CallVoidMethod(env,progressListenerObject,waveListenerId,(jint)db);
            (*env)->CallVoidMethod(env,progressListenerObject,progressListenerId,(jlong)recorder_ms);
        }

        (*javaVm)->DetachCurrentThread(javaVm);

        (*recorderRecorder)->SetRecordState(recorderRecorder,SL_RECORDSTATE_STOPPED);

    } else if (play_status==2){
        (*recorderRecorder)->SetRecordState(recorderRecorder,SL_RECORDSTATE_PAUSED);
    }else if(play_status==1){

        recorder_ms=(frame_size)/(slDataFormatPcm.samplesPerSec/100000);
        JNIEnv* env= NULL;

        (*javaVm)->AttachCurrentThread(javaVm,&env,NULL);

        int result=(*javaVm)->GetEnv((javaVm),(void **)&env,JNI_VERSION_1_4);

        if (result==JNI_OK){
            //LOGE("frame_size %lu recorder_ms%lu",frame_size,recorder_ms);
            if (dbNotify)
                (*env)->CallVoidMethod(env,progressListenerObject,waveListenerId,(jint)db);
            (*env)->CallVoidMethod(env,progressListenerObject,progressListenerId,(jlong)recorder_ms);
        }

        (*javaVm)->DetachCurrentThread(javaVm);

        (*bufferQueue)->Enqueue(bufferQueue,recordBuffer,RECORDER_SIZE);

    }
}

void createEngine(){
    SLresult sLresult= slCreateEngine(&engineObject,
                                      0,
                                      NULL,
                                      0,
                                      NULL,
                                      0);
    if (SL_RESULT_SUCCESS != sLresult){
        return;
    }

    sLresult=(*engineObject)->Realize(engineObject, 0);
    if (SL_RESULT_SUCCESS != sLresult){
        return;
    }
    sLresult=(*engineObject)->GetInterface(engineObject,SL_IID_ENGINE,&audioEngine);
    if (SL_RESULT_SUCCESS != sLresult){
        return;
    }
}


void createRecorder(){
    SLDataLocator_IODevice ioDevice = {SL_DATALOCATOR_IODEVICE,         //类型
                                       SL_IODEVICE_AUDIOINPUT,          //device类型 选择了音频输入类型
                                       SL_DEFAULTDEVICEID_AUDIOINPUT,   //deviceID
                                       NULL                             //device实例
    };
    SLDataSource slDataSource = {
            &ioDevice,
            NULL
    };


    // 设置输出buffer队列
    SLDataLocator_AndroidSimpleBufferQueue buffer_queue = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,    //类型 这里只能是这个常量
            2                                           //buffer的数量
    };

    SLDataSink audioSink = {
            &buffer_queue,                   //SLDataFormat_PCM配置输出
            &slDataFormatPcm                      //输出数据格式
    };

    // 创建录制的对象
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    SLObjectItf recorderObject;
    SLresult sLresult = (*audioEngine)->CreateAudioRecorder(audioEngine,        //引擎接口
                                                            &recorderObject,   //录制对象地址，用于传出对象
                                                            &slDataSource,          //输入配置
                                                            &audioSink,         //输出配置
                                                            1,                  //支持的接口数量
                                                            id,                 //具体的要支持的接口
                                                            req                 //具体的要支持的接口是开放的还是关闭的
    );

    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("创建录制的对象失败...");
        return;
    }
    // 实例化这个录制对象
    sLresult = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);

    if (SL_RESULT_SUCCESS != sLresult) {
        LOGE("实例化这个录制对象失败...");
        return;
    }
    // 获取录制接口
    (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecorder);

    // 获取Buffer接口
    (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                    &recorderBufferQueue);

    sLresult = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, RecordCallback,NULL);

    if (SL_RESULT_SUCCESS != sLresult) {
        return;
    }
    LOGE("Recorder Create OK");
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_init(JNIEnv *env, jobject thiz) {
    slDataFormatPcm.formatType=SL_DATAFORMAT_PCM;
    slDataFormatPcm.numChannels=(SLuint32) 1;
    slDataFormatPcm.samplesPerSec=SL_SAMPLINGRATE_16;
    slDataFormatPcm.bitsPerSample=SL_PCMSAMPLEFORMAT_FIXED_16;
    slDataFormatPcm.containerSize=SL_PCMSAMPLEFORMAT_FIXED_16;
    slDataFormatPcm.channelMask=SL_SPEAKER_FRONT_CENTER;
    slDataFormatPcm.endianness=SL_BYTEORDER_LITTLEENDIAN;
    createEngine();
    createRecorder();
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_setOutputProperty(JNIEnv *env, jobject thiz,jobject recorderParams ) {
    jclass listenerClass=(*env)->GetObjectClass(env,recorderParams);
    jmethodID idSampleRate=(*env)->GetMethodID(env,listenerClass,"getSampleRate", "()J");
    jmethodID idBitFormat=(*env)->GetMethodID(env,listenerClass,"getBitFormat", "()I");
    jmethodID idChannels=(*env)->GetMethodID(env,listenerClass,"getChannels", "()I");
    jmethodID idChannelLayout=(*env)->GetMethodID(env,listenerClass,"getChannelLayout", "()I");
    long sampleRate=(*env)->CallLongMethod(env,recorderParams,idSampleRate);
    int bitFormat=(*env)->CallIntMethod(env,recorderParams,idBitFormat);
    int channels=(*env)->CallIntMethod(env,recorderParams,idChannels);
    int channelLayout=(*env)->CallIntMethod(env,recorderParams,idChannelLayout);
    slDataFormatPcm.numChannels=(SLuint32) channels;
    slDataFormatPcm.samplesPerSec=sampleRate;
    slDataFormatPcm.bitsPerSample=bitFormat;
    slDataFormatPcm.containerSize=bitFormat;
    slDataFormatPcm.channelMask=channelLayout;
    createEngine();
}

JNIEXPORT jobject JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_getOutputProperty(JNIEnv *env, jobject thiz) {
    jclass clazz = (*env)->FindClass(env,"cn/zybwz/dwgame/RecorderParams");
    jmethodID cmethodId=(*env)->GetMethodID(env,clazz,"<init>", "()V");
    return (*env)->NewObject(env,clazz,cmethodId);
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_addProgressListener(JNIEnv *env, jobject thiz,
                                                          jobject listener) {
    LOGE("addProgressListener");
    jclass listenerClass=(*env)->GetObjectClass(env,listener);
    progressListenerId=(*env)->GetMethodID(env,listenerClass,"onProgress", "(J)V");
    waveListenerId=(*env)->GetMethodID(env,listenerClass,"onWave", "(I)V");
    progressListenerObject=(*env)->NewGlobalRef(env,listener);
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_addStatusChangeListener(JNIEnv *env, jobject thiz,
                                                              jobject listener) {

}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_start(JNIEnv *env, jobject thiz, jstring save_path) {
    frame_size=0;
    recorder_ms=0;
    file_size=0;
    char *s=(*env)->GetStringUTFChars(env,save_path,0);
    //pcm_file = fopen(s,"w+");
//    if (pcm_file==NULL){
//        LOGE("file open failed %s",s);
//        return;
//    }

    SLresult sLresult = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer,
                                                        RECORDER_SIZE);
    if (SL_RESULT_SUCCESS != sLresult) {
        return;
    }
    LOGE("rate %d bit %d channel %d",slDataFormatPcm.samplesPerSec,slDataFormatPcm.bitsPerSample,slDataFormatPcm.numChannels);
    sLresult=(*recorderRecorder)->SetRecordState(recorderRecorder, SL_RECORDSTATE_RECORDING);
    if (SL_RESULT_SUCCESS == sLresult)
        play_status = 1;
    (*env)->ReleaseStringUTFChars(env,save_path,s);
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_pause(JNIEnv *env, jobject thiz) {
    play_status = 2;
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_resume(JNIEnv *env, jobject thiz) {
    SLresult sLresult = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer,RECORDER_SIZE);
    if (SL_RESULT_SUCCESS != sLresult) {
        return;
    }
    sLresult=(*recorderRecorder)->SetRecordState(recorderRecorder, SL_RECORDSTATE_RECORDING);
    if (SL_RESULT_SUCCESS == sLresult)
        play_status = 1;
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_stop(JNIEnv *env, jobject thiz) {
    play_status = 3;
}

JNIEXPORT void JNICALL
Java_cn_zybwz_dwgame_OpenSLRecorder_destroy(JNIEnv *env, jobject thiz) {
    engineObject=NULL;
    audioEngine =NULL;
    recorderRecorder=NULL;
    recorderBufferQueue = NULL; //Buffer接口
    (*env)->DeleteGlobalRef(env,progressListenerObject);
    if (pcm_file!=NULL)
        fclose(pcm_file);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){
    javaVm = vm;
    return JNI_VERSION_1_4;
}






