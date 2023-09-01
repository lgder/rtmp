/**
 * @file VideoTask.kt
 * @brief 使用 Android API 采集 YUV 数据, 实时预览在 SurfaceView 上,
 *        并且通过 JNI 传递给 Native 层, 由 Native 层进行 nv21 转换, 编码和推流
 * @author xhunmon
 * @last_editor lq
 */

package com.example.rtmp.core

import android.graphics.ImageFormat
import android.hardware.Camera
import android.hardware.Camera.CameraInfo
import android.hardware.Camera.PreviewCallback
import android.os.Environment
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import java.io.File
import java.io.FileOutputStream

class VideoTask(private val pusher: Pusher) : SurfaceHolder.Callback, PreviewCallback {
    private val holder: SurfaceHolder
    private var camera: Camera? = null
    private lateinit var buffer: ByteArray
    lateinit var bytes: ByteArray
    private var rotation = 0
    var height: Int
    var width: Int

    init {
        holder = pusher.config.surfaceView.holder
        width = pusher.config.width
        height = pusher.config.height
    }

    override fun surfaceCreated(holder: SurfaceHolder) {}
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        stop()
        preview()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        stop()
    }

    override fun onPreviewFrame(data: ByteArray, camera: Camera) {
        when (rotation) {
            Surface.ROTATION_0 -> rotation90(data)
            Surface.ROTATION_90 -> Log.d(TAG, "ROTATION_90 横屏 左边是头部(home键在右边)")
            Surface.ROTATION_270 -> Log.d(TAG, "ROTATION_270 横屏 头部在右边")
            else -> Log.d(TAG, "ROTATION_other")
        }
        if (pusher.isPushing) {
            pusher.native_VideoPush(bytes)
        }
        //注意：旋转之后宽高需要交互，但是不能直接改变全局变量
        camera.addCallbackBuffer(buffer)
    }

    /**
     * codec中对旋转做了以下处理：
     *
     *
     * // 根据rotation判断transform
     * int transform = 0;
     * if ((rotation % 90) == 0) {
     *   switch ((rotation / 90) & 3) {
     *     case 1:  transform = HAL_TRANSFORM_ROT_90;  break;
     *     case 2:  transform = HAL_TRANSFORM_ROT_180; break;
     *     case 3:  transform = HAL_TRANSFORM_ROT_270; break;
     *     default: transform = 0;                     break;
     *   }
     * }
     *
     * 把歪的图片数据变为正的
     */
    private fun rotation90(simple: ByteArray) {
        val ySize = width * height
        val uvHeight = height / 2

        // 后置摄像头，顺时针旋转90
        if (pusher.config.facing == CameraInfo.CAMERA_FACING_BACK) {
            var k = 0
            // 宽高取反，把竖变行
            // y数据
            for (w in 0 until width) {
                for (h in height - 1 downTo 0) {
                    bytes[k++] = simple[h * width + w]
                }
            }
            // uv数据 height*width -> 3/2height*width
            var w = 0
            while (w < width) {
                for (h in uvHeight - 1 downTo 0) {
//                *(simpleOut + k) = simple[width * height + h * width + w];
                    // u
                    bytes[k++] = simple[ySize + width * h + w]
                    // v
                    bytes[k++] = simple[ySize + width * h + w + 1]
                }
                w += 2
            }
        } else {
            // 前置摄像头，顺时针旋转90
            var k = 0
            // y数据
            for (w in width - 1 downTo 0) {
                for (h in 0 until height) {
                    bytes[k++] = simple[h * width + w]
                }
            }
            // uv数据 height*width -> 3/2height*width
            var w = 0
            while (w < width) {
                for (h in uvHeight - 1 downTo 0) {
//                *(simpleOut + k) = simple[width * height + h * width + w];
                    bytes[k++] = simple[ySize + width * h + w]
                    bytes[k++] = simple[ySize + width * h + w + 1]
                }
                w += 2
            }
        }
    }

    // 测试：把传感器的数据输出到本地，使用 ffplay 进行查看，
    // ffplay -f rawvideo -video_size 800x480 xxx.yuv
    private fun testWriteFile(data: ByteArray) {
        if (!isWriteTest) {
            return
        }
        isWriteTest = false
        val path = Environment.getExternalStorageDirectory()
            .toString() + File.separator + "yuv_na21_" + width + "x" + height + "_" + System.currentTimeMillis() + ".yuv"
        val file = File(path)
        try {
            FileOutputStream(file).use { fos ->
                fos.write(data)
                fos.flush()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
        Log.d(TAG, "写入结束：" + file.absolutePath)
    }

    fun preview() {
        try {
            holder.removeCallback(this)
            holder.addCallback(this)
            camera = Camera.open(pusher.config.facing)
            // Parameters这里封装着当前摄像头所能提供的参数（真实宽高等）
            val parameters = camera!!.parameters
            // 设置预览数据为nv21（注意：仅仅是预览的数据，通过onPreviewFrame回调的仍没有发生变化）
            parameters.previewFormat = ImageFormat.NV21
            resetPreviewSize(parameters)
            // 设置摄像头 图像传感器的角度、方向
            setDisplayOrientation()
            camera!!.parameters = parameters
            buffer = ByteArray(width * height * 3 / 2)
            bytes = ByteArray(buffer.size)
            // 数据缓存区
            camera!!.addCallbackBuffer(buffer)
            camera!!.setPreviewCallbackWithBuffer(this)
            // 设置预览画面
            camera!!.setPreviewDisplay(holder)
            camera!!.startPreview()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    // 通过与用户选择的尺寸与手机所提供的对比，重新设置预览尺寸
    private fun resetPreviewSize(parameters: Camera.Parameters) {
        val sizeList = parameters.supportedPreviewSizes
        var preSize: Camera.Size? = null
        var dif = 0
        for (size in sizeList) {
            val tempdif = Math.abs(size.height * size.width - height * width)
            if (preSize == null) {
                preSize = size
                dif = tempdif
            } else {
                // 计算出与用户选择最接近的宽高size
                if (dif > tempdif) {
                    dif = tempdif
                    preSize = size
                }
            }
            Log.d(TAG, "摄像头支持尺寸：w=" + size.width + " | h=" + size.height)
        }
        if (preSize == null) {
            Log.e(TAG, "摄像头支持尺寸获取失败")
            return
        }
        Log.d(TAG, "设置摄像头预览宽高：w=$width | h=$height")
        width = preSize.width
        height = preSize.height
        parameters.setPreviewSize(width, height)
        // 宽高改变了，通知native进行重新设置参数 需要旋转
        val needRotate = rotation == Surface.ROTATION_0
        pusher.native_VideoDataChange(
            if (needRotate) height else width, if (needRotate) width else height
        )
    }

    // 设置摄像头 图像传感器的角度、方向
    private fun setDisplayOrientation() {
        val info = CameraInfo()
        Camera.getCameraInfo(pusher.config.facing, info)
        rotation = pusher.config.activity.windowManager.defaultDisplay.rotation
        var degrees = 0
        when (rotation) {
            Surface.ROTATION_0 -> degrees = 0
            Surface.ROTATION_90 -> degrees = 90
            Surface.ROTATION_180 -> degrees = 180
            Surface.ROTATION_270 -> degrees = 270
            else -> {}
        }
        var result: Int
        if (info.facing == CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + degrees) % 360
            result = (360 - result) % 360 // compensate the mirror
        } else { // back-facing
            result = (info.orientation - degrees + 360) % 360
        }
        // 设置角度
        camera!!.setDisplayOrientation(result)
    }

    fun stop() {
        if (camera != null) {
            camera!!.setPreviewCallback(null)
            camera!!.stopPreview()
            camera!!.release()
            camera = null
        }
    }

    companion object {
        private const val TAG = "VideoTask"
        var isWriteTest = false
    }
}