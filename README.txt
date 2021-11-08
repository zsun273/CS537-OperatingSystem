For each file, we divide the file by the number of threads and zip it concurrently.
To reduce memory allocation, we use a smaller struct for our output if the filesize is smaller than our threshold.
