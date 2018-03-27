#coding=utf-8 

import nlms_numpy
from nlms_numpy import *
import matplotlib.pyplot as pl

def sim_system_identify(nlms, x, h0, step_size, noise_scale):
       y = np.convolve(x, h0)
       d = y + np.random.standard_normal(len(y)) * noise_scale # 添加白色噪声的外部干扰
       h = np.zeros(len(h0), np.float64) # 自适应滤波器的长度和未知系统长度相同，初始值为0
       u = nlms( x, d, h, step_size )
       return y, u, h

def system_identify_test1():
    h0 = make_path(32, 256) # 随机产生一个未知系统的传递函数
    x = np.random.standard_normal(10000)  # 参照信号为白噪声
    y, u, h = sim_system_identify(nlms_numpy.nlms, x, h0, 0.5, 0.1)
    print diff_db(h0, h)
    pl.figure( figsize=(8, 6) )  # 绘图比例8:6
    pl.subplot(211)    #  选中 2x1 个子图的第1个子图
    pl.subplots_adjust(hspace=0.4)
    pl.plot(h0, c="r") # r 是 红色
    pl.plot(h, c="b")  # b 是 蓝色
    pl.title(u"未知系统和收敛后的滤波器的系数比较")
    pl.subplot(212)    #  选中 2x1 个子图的第2个子图
    plot_converge(y, u)
    pl.title(u"自适应滤波器收敛特性")
    pl.xlabel("Iterations (samples)")
    pl.ylabel("Converge Level (dB)")
    pl.show()

def system_identify_test1_1():
    h0 = make_path(32, 256) # 随机产生一个未知系统的传递函数
    x = np.random.standard_normal(10000)  # 参照信号为白噪声
    y, u, h = sim_system_identify(nlms_numpy.nlms, x, h0, 0.5, 0.1)
    print diff_db(h0, h)
    pl.figure( figsize=(8, 6) )  # 绘图比例8:6

    pl.subplot(411)    
    pl.subplots_adjust(hspace=0.4)
    pl.plot(h0, c="r") # r 是 红色
    pl.plot(h, c="b")  # b 是 蓝色
    pl.title(u"未知系统和收敛后的滤波器的系数比较")

    pl.subplot(412)   
    pl.subplots_adjust(hspace=0.4)
    pl.plot(y, c="r") # r 是 红色
    pl.plot(u, c="b")  # b 是 蓝色
    pl.title(u"未知系统和滤波器输出比较")

    pl.subplot(413)   
    pl.subplots_adjust(hspace=0.4)
    usize = len(u)
    e = y[:usize] - u
    pl.plot(e, c="g") 
    pl.title(u"未知系统和滤波器误差")


    pl.subplot(414)    
    plot_converge(y, u)
    pl.title(u"自适应滤波器收敛特性")
    pl.xlabel("Iterations (samples)")
    pl.ylabel("Converge Level (dB)")

    pl.show()


def system_identify_test2():
    h0 = make_path(32, 256) # 随机产生一个未知系统的传递函数
    x = np.random.standard_normal(20000)  # 参照信号为白噪声
    pl.figure(figsize=(8,4))
    for step_size in np.arange(0.1, 1.0, 0.2):
        y, u, h = sim_system_identify(nlms_numpy.nlms, x, h0, step_size, 0.1)
        plot_converge(y, u, label=u"μ=%s" % step_size)
    pl.title(u"更新系数和收敛特性的关系")
    pl.xlabel("Iterations (samples)")
    pl.ylabel("Converge Level (dB)")
    pl.legend()
    pl.show()

def system_identify_test3():
    h0 = make_path(32, 256) # 随机产生一个未知系统的传递函数
    x = np.random.standard_normal(20000)  # 参照信号为白噪声
    pl.figure(figsize=(8,4))
    for noise_scale in [0.05, 0.1, 0.2, 0.4, 0.8]:
        y, u, h = sim_system_identify(nlms_numpy.nlms, x, h0, 0.5, noise_scale)
        plot_converge(y, u, label=u"noise=%s" % noise_scale)
    pl.title(u"外部干扰和收敛特性的关系")
    pl.xlabel("Iterations (samples)")
    pl.ylabel("Converge Level (dB)")
    pl.legend()
    pl.show()

system_identify_test1_1()

