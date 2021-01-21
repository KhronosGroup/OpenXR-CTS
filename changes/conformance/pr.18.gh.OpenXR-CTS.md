Lower the amount of time that the renderer blocks. The CTS is not a highly
optimized application and due to thread scheduling and extra GPU waits 90% CPU
wait makes it fairly tight to fit everything inside of a single display period.
