# c-ffmpeg

C-ffmpeg是基于ffmpeg的库封装的流处理函数库，类似ffmpeg和ffplay命令工具。用于给其它语言如：golang和java，提供类似ffmpeg命令功能的函数。

## 编译和安装

```shell
make build
cd build
cmake ..
make
```



## 函数说明

### push2rtsp_sub_logo

 * 拉取视频流（流媒体或者视频文件）通过rtsp协议推送到直播服务器。
 * 支持多种类型的字幕解析，并硬编字幕到输出流。
 * 支持外挂视频logo，并能够周期性淡入淡出。
 * 支持NVIDIA的硬解编码实现。（与push2trtsp_sub_logo_cuda的区别：通过NVIDIA的硬件解码能力，解码视频帧，再下载到CPU后进行overlay处理，再上传至GPU进行编码，视频帧有两次GPU和CPU之间的数据传输，受限于GPU和CPU之间的传输带宽限制，对于4k和60fps以上的高清数据帧传输效率影响。）
 * 支持推流任务的播放控制，如：快进/快退/暂停/继续/取消等。

#### 参数

| **参数名**               | **类型** | **默认值** | **说明**                     |
| ------------------------ | -------- | ---------- | ---------------------------- |
| video_file_path          | char*    | ""         | 视频文件或流的采集地址       |
| video_index              | int      | 0 | 选择播放的文件或流中的视频流索引 |
| audio_index              | int | 0 | 选择播放的文件或流中的音频流索引 |
| subtitle_index           | int | 0 | 选择播放的文件或流中的字幕流索引 |
| subtitle_file_path       | char* | "" | 外挂字幕文件的地址 |
| logo_frame               | AVFrame* | NULL | 视频LOGO的图像帧 |
| rtsp_pusth_path          | char* | "" | 推流的rtsp地址 |
| if_hw             | bool | true | 是否硬件加速，目前支持NVIDIA的编解码 |
| if_logo_fade           | bool | true | 是否LOGO图像淡入淡出 |
| duration_frames          | uint64_t | 0 | logo淡入淡入持续帧数 |
| interval_frames          | uint64_t | 0 | logo淡入淡入间隔帧数 |
| present_frames           | uint64_t | 0 | logo显示的持续帧数 |
| task_handle_process_info | TaskHandleProcessInfo‎ * | NULL | 推流任务控制信息 |



------



### push2rtsp_sub_logo_cuda

- 拉取视频流（流媒体或者视频文件）通过rtsp协议推送到直播服务器。
- 支持多种类型的字幕解析，并硬编字幕到输出流。
- 支持外挂视频logo，并能够周期性淡入淡出。
- 支持NVIDIA的硬解编码实现。（与push2trtsp_sub_logo的区别：只在GPU内编解码视频帧，解码后帧不交换到CPU内进行视频处理，logo和subtitle图片都上传到GPU的显存中进行overlay，从而避免高质量视频帧在GPU->CPU->GPU转换过程中的大量数据传输，提升4k和60fps上高清视频编解码和overlay处理的速率。）
- 支持推流任务的播放控制，如：快进/快退/暂停/继续/取消等。

#### 参数

| **参数名**               | **类型**                | **默认值** | **说明**                             |
| ------------------------ | ----------------------- | ---------- | ------------------------------------ |
| video_file_path          | char*                   | ""         | 视频文件或流的采集地址               |
| video_index              | int                     | 0          | 选择播放的文件或流中的视频流索引     |
| audio_index              | int                     | 0          | 选择播放的文件或流中的音频流索引     |
| subtitle_index           | int                     | 0          | 选择播放的文件或流中的字幕流索引     |
| subtitle_file_path       | char*                   | ""         | 外挂字幕文件的地址                   |
| logo_frame               | AVFrame*                | NULL       | 视频LOGO的图像帧                     |
| rtsp_pusth_path          | char*                   | ""         | 推流的rtsp地址                       |
| if_hw                    | bool                    | true       | 是否硬件加速，目前支持NVIDIA的编解码 |
| if_logo_fade             | bool                    | true       | 是否LOGO图像淡入淡出                 |
| duration_frames          | uint64_t                | 0          | logo淡入淡入持续帧数                 |
| interval_frames          | uint64_t                | 0          | logo淡入淡入间隔帧数                 |
| present_frames           | uint64_t                | 0          | logo显示的持续帧数                   |
| task_handle_process_info | TaskHandleProcessInfo‎ * | NULL       | 推流任务控制信息                     |



------



### push2rtsp_sub_logo_cuda_thread

- 拉取视频流（流媒体或者视频文件）通过rtsp协议推送到直播服务器。
- 支持多种类型的字幕解析，并硬编字幕到输出流。
- 支持外挂视频logo，并能够周期性淡入淡出。
- 支持NVIDIA的硬解编码实现。（与push2trtsp_sub_logo的区别：只在GPU内编解码视频帧，解码后帧不交换到CPU内进行视频处理，logo和subtitle图片都上传到GPU的显存中进行overlay，从而避免高质量视频帧在GPU->CPU->GPU转换过程中的大量数据传输，提升4k和60fps上高清视频编解码和overlay处理的速率。）
- 支持推流任务的播放控制，如：快进/快退/暂停/继续/取消等。
- 支持音频和视频的解码编码通过分开线程处理，避免音频和视频编码之间的相互影响。

#### 参数

| **参数名**               | **类型**                | **默认值** | **说明**                             |
| ------------------------ | ----------------------- | ---------- | ------------------------------------ |
| video_file_path          | char*                   | ""         | 视频文件或流的采集地址               |
| video_index              | int                     | 0          | 选择播放的文件或流中的视频流索引     |
| audio_index              | int                     | 0          | 选择播放的文件或流中的音频流索引     |
| subtitle_index           | int                     | 0          | 选择播放的文件或流中的字幕流索引     |
| subtitle_file_path       | char*                   | ""         | 外挂字幕文件的地址                   |
| logo_frame               | AVFrame*                | NULL       | 视频LOGO的图像帧                     |
| rtsp_pusth_path          | char*                   | ""         | 推流的rtsp地址                       |
| if_hw                    | bool                    | true       | 是否硬件加速，目前支持NVIDIA的编解码 |
| if_logo_fade             | bool                    | true       | 是否LOGO图像淡入淡出                 |
| duration_frames          | uint64_t                | 0          | logo淡入淡入持续帧数                 |
| interval_frames          | uint64_t                | 0          | logo淡入淡入间隔帧数                 |
| present_frames           | uint64_t                | 0          | logo显示的持续帧数                   |
| task_handle_process_info | TaskHandleProcessInfo‎ * | NULL       | 推流任务控制信息                     |



------

