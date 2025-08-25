1、先执行make build_third 构建第三方库 openssl、opencv、zlmediakit
      也可以单独执行 make build_opencv   make build_openssl   make build_zlmediakit  单独编译
2、进入thidr_party目录，执行 sh Galaxy_camera.run 按照提示执行后续步骤，解压得到摄像头库文件 复制到mpp/out/third/Galaxy_camera/目录下
3、执行 make build_projet 编译

4、执行make clean_third删除第三方库的编译产物
