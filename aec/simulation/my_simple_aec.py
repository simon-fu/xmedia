#coding=utf-8 

import nlms_numpy
from nlms_numpy import *
# import matplotlib.pyplot as plt
import wave  
import pylab as pl  
import numpy   

def sim_system_identify(nlms, x, h0, step_size, noise_scale):
       y = np.convolve(x, h0)
       d = y + np.random.standard_normal(len(y)) * noise_scale # 添加白色噪声的外部干扰
       h = np.zeros(len(h0), np.float64) # 自适应滤波器的长度和未知系统长度相同，初始值为0
       u = nlms( x, d, h, step_size )
       return y, u, h




  
#打开wav文件  
#open返回一个的是一个Wave_read类的实例，通过调用它的方法读取WAV文件的格式和数据  
f = wave.open(r"/Users/simon/easemob/src/xmedia/aec/tmp-matlab/ref.wav","rb")  
# f = wave.open(r"/Users/simon/easemob/src/xmedia/aec/simulation/channels2.wav","rb")  

  
#读取格式信息  
#一次性返回所有的WAV文件的格式信息，它返回的是一个组元(tuple)：声道数, 量化位数（byte单位）, 采  
#样频率, 采样点数, 压缩类型, 压缩类型的描述。wave模块只支持非压缩的数据，因此可以忽略最后两个信息  
params = f.getparams()  
nchannels, bytesPerSample, sampleRate, nframes = params[:4]  
print 'nchannels=', nchannels
print 'bytesPerSample=', bytesPerSample
print 'sampleRate=', sampleRate
print 'nframes=', nframes
  
#读取波形数据  
#读取声音数据，传递一个参数指定需要读取的长度（以取样点为单位）  
str_data  = f.readframes(nframes)  
f.close()  
  
#将波形数据转换成数组  
#需要根据声道数和量化单位，将读取的二进制数据转换为一个可以计算的数组  
wave_data = numpy.fromstring(str_data,dtype = numpy.short)  
wave_data.shape = -1,2  
wave_data = wave_data.T  
# time = numpy.arange(0,nframes)*(1.0/sampleRate)  
# len_time = len(time)/2  
# time = time[0:len_time]  
  
##print "time length = ",len(time)  
##print "wave_data[0] length = ",len(wave_data[0])  

print 'len_data0', len(wave_data[0])
print 'len_data1', len(wave_data[1])

#绘制波形  
  
# pl.subplot(211)  
# pl.plot(time,wave_data[0])  
# pl.subplot(212)  
# pl.plot(time, wave_data[1],c="r")  
# pl.xlabel("time")  
# pl.show() 

pl.subplot(211)  
pl.plot(wave_data[0])  
pl.subplot(212)  
pl.plot(wave_data[1],c="r")  
pl.xlabel("time")  
pl.show()


