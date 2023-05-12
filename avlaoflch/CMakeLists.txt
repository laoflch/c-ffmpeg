
aux_source_directory(. DIR_LIB_AV_LAOFLCH)

set(FFMPEG_LIB_PREFIX /workspace/ffmpeg/lib)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavcodec.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavutil.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavfilter.so)
list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavformat.so)



#aux_source_directory(/workspace/ffmpeg/lib DIR_LIB_FFMPEG)
add_library( avlaoflch SHARED ${DIR_LIB_AV_LAOFLCH} )
#add_library( avlaoflch_static STATIC ${DIR_LIB_AV_LAOFLCH} )
#link_libraries("/workspace/ffmpeg/lib")
target_link_libraries(avlaoflch ${FFMPEG_LIB})



#set(FFMPEG_LIB)

#find_package(libavcodec REQUIRED)

#list(APPEND FFMPEG_LIB ${FFMPEG_LIB_PREFIX}/libavcodec.so)
include_directories(/workspace/ffmpeg/FFmpeg)
add_executable(ComplexFilter complex_filter.c )

#target_link_directories(ComplexFilter PUBLIC  /workspace/ffmpeg/lib )

#target_link_libraries(ComplexFilter /workspace/ffmpeg/lib/libavcodec.so /workspace/ffmpeg/lib/libavutil.so /workspace/ffmpeg/lib/libavfilter.so /workspace/ffmpeg/lib/libavformat.so  ${libavlaoflch})
target_link_libraries(ComplexFilter ${FFMPEG_LIB})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/complex_filter.h DESTINATION ${CMAKE_BINARY_DIR}/avlaoflch)
