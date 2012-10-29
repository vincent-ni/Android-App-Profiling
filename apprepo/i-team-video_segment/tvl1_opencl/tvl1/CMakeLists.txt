add_library(tvl1 tvl1.cpp)
target_link_libraries(tvl1 timer simplecl ${OpenCV_LIBS} ${CUDA_LIBS} ${OPENCL_LIBRARIES})
