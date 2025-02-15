
aux_source_directory(. DIR_LIB_AV_LAOFLCH)

set(FFMPEG_LIB_PREFIX /workspace/ffmpeg/lib)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavcodec.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavcodec.so.59)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavutil.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavutil.so.57)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavfilter.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavfilter.so.8)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavfilter.a)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libswscale.so.5)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libswscale.so.6)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavformat.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavformat.so.59)
##list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libswresample.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libswresample.so.4)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libx264.so.155)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libx265.so.179)

list(APPEND FFMPEG_LIB /lib/x86_64-linux-gnu/libass.so.9)
list(APPEND FFMPEG_LIB /lib/x86_64-linux-gnu/libm.so.6)

list(APPEND FFMPEG_LIB_THREAD /lib/x86_64-linux-gnu/libpthread-2.31.so)


#aux_source_directory(/workspace/ffmpeg/lib DIR_LIB_FFMPEG)
add_library( avlaoflch SHARED  push_stream_cuda.c push_stream.c complex_filter.c subtitle.c )
#add_library( avlaoflch_static STATIC ${DIR_LIB_AV_LAOFLCH} )
#link_libraries("/workspace/ffmpeg/lib")
target_link_libraries(avlaoflch ${FFMPEG_LIB})



#set(FFMPEG_LIB)

#find_package(libavcodec REQUIRED)

#list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavcodec.so)
include_directories(/workspace/ffmpeg/ffmpeg_docker/FFmpeg)
#include_directories(complex_filter.h)
#include_directories(subtitle.h)

#add_executable(ComplexFilter complex_filter.c drawutils.c )
#add_executable(PushStream push_stream.c complex_filter.c subtitle.c) 

add_executable(PushStreamCudaThread push_stream_cuda_thread.c  push_stream.c complex_filter.c subtitle.c) 
add_executable(PushStreamCuda push_stream_cuda.c push_stream.c complex_filter.c subtitle.c) 
#target_link_directories(ComplexFilter PUBLIC  /workspace/ffmpeg/lib )

#target_link_libraries(ComplexFilter /workspace/ffmpeg/lib/libavcodec.so /workspace/ffmpeg/lib/libavutil.so /workspace/ffmpeg/lib/libavfilter.so /workspace/ffmpeg/lib/libavformat.so  ${libavlaoflch})
#target_link_libraries(ComplexFilter ${FFMPEG_LIB})

target_link_libraries(PushStreamCudaThread ${FFMPEG_LIB} ${FFMPEG_LIB_THREAD} )
target_link_libraries(PushStreamCuda ${FFMPEG_LIB})
#target_link_libraries(PushStream ${FFMPEG_LIB})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/complex_filter.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/push_stream.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/push_stream_cuda.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/subtitle.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/drawutils.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
