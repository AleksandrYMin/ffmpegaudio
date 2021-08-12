# ffmpegaudio
C++ Class for reading and writing audiofile using ffmpeg library. <br>
open_file() - returns std::vector<double> with mono audiodata resampled to 16 kHz<br>
int createOutputFile(const char filename[]) - opens wav file for writing 16kHz, 16 bits, mono<br>
int closeOutputFile() - writes trailer and closes file<br>
int writeData(std::vector<double> data) - writes data to file<br>
