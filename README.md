# c-ffmpeg

C-ffmpeg是基于ffmpeg的库封装的流处理函数库，类似ffmpeg和ffplay命令工具。用于给其它语言如：golang和java，提供类似ffmpeg命令功能的函数。

## 编译和安装

------

```shell
make build
cd build
cmake ..
make
```



## 函数说明

### push2trtsp_sub_logo

 * 拉取视频流（流媒体或者视频文件）通过rtsp协议推送到直播服务器。
 * 支持多种类型的字幕解析，并硬编字幕到输出流。
 * 支持外挂视频logo，并能够周期性淡入淡出。
 * 支持NVIDIA的硬解编码实现。
 * 支持推流任务的播放控制，如：快进/快退/暂停/继续/取消等。