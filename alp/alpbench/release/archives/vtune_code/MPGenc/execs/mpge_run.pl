#!/usr/bin/perl

$mpge_sse = "/home/adve_s/manlapli/more_space/release/vtune_code/MPGenc/execs/mpeg2enc_sse";
$mpge_nosse = "/home/adve_s/manlapli/more_space/release/vtune_code/MPGenc/execs/mpeg2enc_nosse";

for ($i=0; $i<10;$i++) {
  system($mpge_nosse." ~/more_space/video_sample/tens_40.par blah.m2v");
  system("gprof ".$mpge_nosse. " > nosse".$i);
  system($mpge_sse." ~/more_space/video_sample/tens_40.par blah.m2v");
 system("gprof ".$mpge_sse. " > sse".$i);
}

