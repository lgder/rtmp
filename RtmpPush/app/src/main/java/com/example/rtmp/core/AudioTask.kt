/**
 * @file AudioTask.kt
 * @brief 使用 Android API 录音
 * @author xhunmon
 * @last_editor lq
 * @note
 */

package com.example.rtmp.core

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class AudioTask(private val pusher: Pusher) {
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val audioRecord: AudioRecord?
    private var minBufferSize = 0

    init {
        //准备录音机 采集 pcm 数据
        val channelConfig: Int = if (pusher.config.channels == 2) {
            AudioFormat.CHANNEL_IN_STEREO
        } else {
            AudioFormat.CHANNEL_IN_MONO
        }

        //通过faac框架获取缓冲区大小（16 位 2个字节）
        val inputSamples = pusher.native_AudioGetSamples() * 2

        //android系统提供的：最小需要的缓冲区；
        minBufferSize = AudioRecord.getMinBufferSize(
            pusher.config.sampleRate,
            AudioFormat.CHANNEL_IN_DEFAULT,
            AudioFormat.ENCODING_PCM_16BIT
        ) * 2
        //说明设备不支持用户所取的采样率
        if (minBufferSize <= 0) {
            //通过api查询设备是否支持该采样率
            for (rate in intArrayOf(44100, 22050, 11025, 16000, 8000)) {
                //最小需要的缓冲区
                minBufferSize = AudioRecord.getMinBufferSize(
                    rate, AudioFormat.CHANNEL_IN_DEFAULT, AudioFormat.ENCODING_PCM_16BIT
                ) * 2
                if (minBufferSize > 0) {
                    pusher.config.sampleRate = rate
                    break
                }
            }
        }
        Log.d(TAG, "minBufferSize：$minBufferSize | inputSamples：$inputSamples")
        if (inputSamples in 1..<minBufferSize) {
            minBufferSize = inputSamples
        }

        //设备采样率有限
        if (minBufferSize <= 0) {
            Log.e(TAG, "设备采样率有限~~~~~~~~~~")
        }

        // 1、麦克风 2、采样率 3、声道数 4、采样位
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            pusher.config.sampleRate,
            channelConfig,
            AudioFormat.ENCODING_PCM_16BIT,
            minBufferSize
        )
    }

    fun push() {
        // 实时直播声音不能预览, 开始推流的时候才开始录音
        executor.execute(RecordingTask())
    }

    fun stop() {
        audioRecord?.stop()
    }

    // 新线程中启动录音机, 录制声音并编码
    internal inner class RecordingTask : Runnable {
        override fun run() {
            audioRecord!!.startRecording()
            val bytes = ByteArray(minBufferSize)
            while (pusher.isPushing) {
                val len = audioRecord.read(bytes, 0, bytes.size)
                if (len > 0) {
                    pusher.native_AudioPush(bytes)
                }
            }
        }
    }

    companion object {
        private const val TAG = "AudioTask"
    }
}
