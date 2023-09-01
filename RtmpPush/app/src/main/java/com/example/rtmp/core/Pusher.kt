/**
 * @file Pusher.kt
 * @brief
 * @author xhunmon
 * @last_editor lq
 */

package com.example.rtmp.core

import android.app.Activity
import android.hardware.Camera.CameraInfo
import android.view.SurfaceView

class Pusher private constructor(var config: Config) {
    external fun native_Init(fps: Int, bitRate: Int, sampleRate: Int, channels: Int)
    external fun native_Prepare(url: String?)
    external fun native_Stop()
    external fun native_AudioGetSamples(): Int
    external fun native_AudioPush(bytes: ByteArray?)
    external fun native_VideoDataChange(width: Int, height: Int)
    external fun native_VideoPush(bytes: ByteArray?)

    private val videoTask: VideoTask
    private val audioTask: AudioTask

    private var isPreviewing = false
    var isPushing = false

    init {
        native_Init(config.fps, config.bitRate, config.sampleRate, config.channels)
        videoTask = VideoTask(this)
        audioTask = AudioTask(this)
    }

    @Synchronized
    fun preview(): Boolean {
        if (isPreviewing) {
            return false
        }
        isPreviewing = true
        videoTask.preview()
        return true
    }

    @Synchronized
    fun push(): Boolean {
        if (isPushing) {
            return false
        }
        if (!isPreviewing) {
            videoTask.preview()
        }
        // 通知native马上要开始
        native_Prepare(config.url)
        // 设置推送开关
        isPreviewing = true
        isPushing = true
        audioTask.push()
        return true
    }

    @Synchronized
    fun stop(): Boolean {
        if (!isPushing && !isPreviewing) {
            return false
        }
        native_Stop()
        isPreviewing = false
        isPushing = false
        videoTask.stop()
        audioTask.stop()
        return true
    }

    @Synchronized
    fun switchCamera() {
        config.facing =
            if (config.facing == CameraInfo.CAMERA_FACING_BACK) CameraInfo.CAMERA_FACING_FRONT else CameraInfo.CAMERA_FACING_BACK
        videoTask.stop()
        videoTask.preview()
    }

    // 默认设置
    class Config(var activity: Activity, var surfaceView: SurfaceView, var url: String) {
        // 这是用户的宽高，与相机真实提供的可能并不一样
        var height = 1280
        var width = 720

        // 码率(也称比特率，指单位时间内连续播放的媒体的比特数量；文件大小=码率 x 时长)
        var bitRate = 800000

        // 帧率
        var fps = 5

        // 前置或者后置摄像头
        var facing = CameraInfo.CAMERA_FACING_BACK

        // 声道数；默认双声道
        var channels = 2

        // 音频采样率
        var sampleRate = 44100

        fun height(height: Int): Config {
            if (pusher != null) {
                return this
            }
            this.height = height
            return this
        }

        fun width(width: Int): Config {
            if (pusher != null) {
                return this
            }
            this.width = width
            return this
        }

        fun bitRate(bitRate: Int): Config {
            if (pusher != null) {
                return this
            }
            this.bitRate = bitRate
            return this
        }

        fun facing(facing: Int): Config {
            if (pusher != null) {
                return this
            }
            this.facing = facing
            return this
        }

        fun fps(fps: Int): Config {
            if (pusher != null) {
                return this
            }
            this.fps = fps
            return this
        }

        fun channels(channels: Int): Config {
            if (pusher != null) {
                return this
            }
            this.channels = channels
            return this
        }

        fun sampleRate(sampleRate: Int): Config {
            if (pusher != null) {
                return this
            }
            this.sampleRate = sampleRate
            return this
        }

        fun build(): Pusher? {
            if (pusher == null) {
                pusher = Pusher(this)
            }
            return pusher
        }
    }

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
        private var pusher: Pusher? = null
    }
}