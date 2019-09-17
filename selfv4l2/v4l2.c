#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <cutils/log.h>

#include <linux/videodev2.h>

#define SKY_LOG(str) SLOGD( \
    "sky log FuncName::%s Line::%d Msg::%s Errorinfo::%s \n", \
    __func__, __LINE__, str, strerror(errno)) 

// Camera Capture Size
#define CAMERA_CAPTURE_WIDTH  (1280)
#define CAMERA_CAPTURE_HEIGHT (720)
#define CAPTURE_PIXELFORMAT (V4L2_PIX_FMT_NV16)
#define USE_BUF_COUNT (4)
#define MAX_BUF_COUNT (4)
#define MAX_FRAME_COUNT 200

void open_device();
void init_device();
void init_userptr();
void close_device();
void sleep_ms(unsigned int sec);
void mainloop();
int px_ioctrl(int fd, int request, void* arg);
void start_capturing();
void read_frame();
void get_image(const void* ptr, int length);

char *deviceName = "/dev/video0";
int fd;
unsigned int n_buffers;
int frame_num = 0;

//初始化这个要画出的矩形大小
struct v4l2_rect cap_rect = {
    0,                    // Left
    0,                    // Top
    CAMERA_CAPTURE_WIDTH, // Width
    CAMERA_CAPTURE_HEIGHT // Height
};

struct buffer {
        void   *start;
        size_t  length;
};

struct buffer* buffers;

int main()
{
    //检查设备,打开设备
    open_device();
    
    //初始化设备
    init_device();

    //初始化buffer
    init_userptr();
    
    //开始
    start_capturing();

    mainloop();

    //关掉设备
    close_device();

    return 0;
}

void open_device()
{
    SLOGD("sky open device");
    if (fd == -1) {
        SKY_LOG("open device error");
    }

    //check state
    struct stat st;
    if (-1 == stat(deviceName, &st)) {
        SKY_LOG("check state error");
    }

    //check 是否是一个字符设备
    // S_ISLNK(st_mode)：是否是一个连接.
    // S_ISREG(st_mode)：是否是一个常规文件.
    // S_ISDIR(st_mode)：是否是一个目录
    // S_ISCHR(st_mode)：是否是一个字符设备.
    // S_ISBLK(st_mode)：是否是一个块设备
    // S_ISFIFO(st_mode)：是否 是一个FIFO文件.
    // S_ISSOCK(st_mode)：是否是一个SOCKET文件 
    if (!S_ISCHR(st.st_mode)) {
        SKY_LOG("It is not a device, error");
    }

    //open /dev/video0
    fd = open(deviceName, O_RDWR | O_NONBLOCK, 0);
    printf("the fd is :: %d", fd);
    if (fd < 0){
        SKY_LOG("open dev/video0 error");
    }
}

void close_device() 
{
    SLOGD("sky close device");
    int close_id = close(fd);

    if (-1 ==  close_id){
        SKY_LOG("close error");
    }
}

void init_device()
{
    SLOGD("sky init device");
    // VIDIOC_REQBUFS：分配内存
    // VIDIOC_QUERYBUF：把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
    // VIDIOC_QUERYCAP：查询驱动功能
    // VIDIOC_ENUM_FMT：获取当前驱动支持的视频格式
    // VIDIOC_S_FMT：设置当前驱动的频捕获格式
    // VIDIOC_G_FMT：读取当前驱动的频捕获格式
    // VIDIOC_TRY_FMT：验证当前驱动的显示格式
    // VIDIOC_CROPCAP：查询驱动的修剪能力
    // VIDIOC_S_CROP：设置视频信号的边框
    // VIDIOC_G_CROP：读取视频信号的边框
    // VIDIOC_QBUF：把数据从缓存中读取出来
    // VIDIOC_DQBUF：把数据放回缓存队列
    // VIDIOC_STREAMON：开始视频显示函数
    // VIDIOC_STREAMOFF：结束视频显示函数
    // VIDIOC_QUERYSTD：检查当前视频设备支持的标准，例如PAL或NTSC。
    struct v4l2_capability cap; //这个设备的功能，比如是否是视频输入设备
    struct v4l2_cropcap cropcap; //裁剪能力
    struct v4l2_crop crop; // 裁剪
    struct v4l2_format fmt;  // 设置视频捕获格式
    struct v4l2_input input; //视频输入

    //查询能力
    if (-1 == px_ioctrl(fd, VIDIOC_QUERYCAP, &cap)){
        SKY_LOG("VIDIOC_QUERYCAP ERROR  ");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        SKY_LOG("can't capture for V4L2_CAP_VIDEO_CAPTURE");
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        SKY_LOG("V4L2_CAP_STREAMING error");
    }

    //把结构体所占的内存全部初始化为0, 方便以后的赋值操作
    //这个地方可以定义一个宏函数暂时先这样
    memset(&input, 0x00, sizeof(struct v4l2_input));
    memset(&cropcap, 0x00, sizeof(struct v4l2_cropcap));
    memset(&fmt, 0, sizeof(struct v4l2_format));
    
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = cap_rect.width;
    fmt.fmt.pix.height      = cap_rect.height;
    fmt.fmt.pix.pixelformat = CAPTURE_PIXELFORMAT;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED_BT;

    //设置图像的格式
    int result = px_ioctrl(fd, VIDIOC_TRY_FMT, &fmt);
    if (result < 0){
        SKY_LOG("VIDIOC_TRY_FMT error");
    }

    result = px_ioctrl(fd, VIDIOC_S_FMT, &fmt);
    
    if (result < 0){
        SKY_LOG("VIDIOC_S_FMT error");
    }
    
    memset(&crop, 0x00, sizeof(struct v4l2_crop));
    crop.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.left   = cropcap.defrect.left;
    crop.c.top    = cropcap.defrect.top;
    crop.c.width  = cropcap.defrect.width;
    crop.c.height = cropcap.defrect.height;

    result = px_ioctrl(fd, VIDIOC_S_CROP, &crop);
    if (result < 0){
        SKY_LOG("VIDIOC_S_CROP error");
    }
    
}

void init_userptr()
{
    SLOGD("sky init userptr");
    int result;
    struct v4l2_requestbuffers req;
    memset(&req, 0x00, sizeof(struct v4l2_requestbuffers));

    req.count = USE_BUF_COUNT; // buffer size if 4
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // video capture type
    req.memory = V4L2_MEMORY_MMAP; //mmap 方式

    result = px_ioctrl(fd, VIDIOC_REQBUFS, &req); //申请空间

    if (req.count < 2) {
        SKY_LOG("insufficient buf memory\n"); //不足的
    }

    if (req.count > USE_BUF_COUNT){
        SKY_LOG("request buf memory\n"); //申请error
    }
    
    buffers = (struct buffer*)calloc(req.count, sizeof(*buffers));

    for (n_buffers = 0; n_buffers < req.count; n_buffers++)
    {
        struct v4l2_buffer buf;
        //clear buffer
        memset(&buf, 0x00, sizeof(struct v4l2_buffer));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers;

        //查询分配的buffer信息
        int result = px_ioctrl(fd, VIDIOC_QUERYBUF, &buf);
        if (-1 == result)
        {
            SKY_LOG("VIDIOC_QUERYBUF error");
        }

        //void *mmap(void *start, size_t length, int prot, int flags,
        //int fd, off_t offset);
        //start:: 映射区的开始地址
        //length:: 映射区的长度
        //prot :: 期望的内存保护标志，不能与文件的打开模式冲突
        //flags:: 指定映射类型
        //offset：被映射对象内容的起点。

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = 
        mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
   }
}

void start_capturing()
{
    SLOGD("sky start capturing");
    enum v4l2_buf_type type;
    int result;
    //n_buffers 在初始化buffer的时候已经自动+1了
    for (size_t i = 0; i < n_buffers; i++){
        struct v4l2_buffer buf;
        memset(&buf, 0x00, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        //VIDIOC_QBUF 把buffer放入放进输入队列，等待填充数据
        result = px_ioctrl(fd, VIDIOC_QBUF, &buf);
        if (-1 == result){
            SKY_LOG("VIDIOC_QBUF error");
            return;
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //开始
    result = px_ioctrl(fd, VIDIOC_STREAMON, &type);
    if (-1 == result){
        SKY_LOG("VIDIOC_STREAMON error");
    }
}

void mainloop()
{
    SLOGD("sky start main loop");
    unsigned int count;
    count = MAX_FRAME_COUNT;  //获取200帧数据

    while (count -- >0)
    {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            tv.tv_sec = 2;
            tv.tv_usec = 0;

            //检测摄像头是否有数据可以读取
            //select 和 poll 来对文件描述符进行监听是否有数据可读。
            r = select(fd+1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                SKY_LOG("system error");
            }

            if (0 == r) {
                SKY_LOG("select time out");
            }

            read_frame();
            
            
        }
        
    }
    
}

void read_frame()
{
    SLOGD("sky read frame");
    struct v4l2_buffer buf;
    memset(&buf, 0x00, sizeof(struct v4l2_buffer));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    size_t result;
    result = px_ioctrl(fd, VIDIOC_DQBUF, &buf);
    if (-1 == result) {
        SKY_LOG("DQBUF error");
    }
    
    get_image(buffers[buf.index].start, buf.bytesused);

    if (-1 == px_ioctrl(fd, VIDIOC_QBUF, &buf))
    SKY_LOG("VIDIOC_QBUF error");
}

void get_image(const void* ptr, int length)
{
    SLOGD("sky get image");
    frame_num++;
    char filename[20];
    sprintf(filename, "frame-%d.raw", frame_num);
    FILE *fp=fopen(filename,"wb"); //文件名字，后面的是模式
    fwrite(ptr, length, 1, fp);

    fflush(ptr);
    fclose(ptr);
}

int px_ioctrl(int fd, int request, void* arg)
{
    int r;
    do
    {
       r = ioctl(fd, request, arg);
    } while (-1 == r);
    
    return r;
}