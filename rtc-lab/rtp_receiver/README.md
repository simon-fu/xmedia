RUN
---
To run this demo, you will need following requisites installed:

- tcpreplay:  https://tcpreplay.appneta.com/
- cmake:      https://cmake.org/
- libopus:    https://opus-codec.org/ 

After all requisites installed, you can compile and run using: 
<pre>
>./compile_and_run.sh
</pre>

It will replay rtp packets captured to localhost:63629, 
Then, rtp_receiver decodes those packets and save them into out.2ch.16000.s16le.

You can play the output file using ffplay or other player:
<pre>
>ffplay -f s16le -ac 2 -ar 16000 ./out.2ch.16000.s16le
</pre> 


