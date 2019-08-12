git clone https://github.com/resibots/limbo limbo
cd limbo
./waf configure
./waf build
./waf --create test
./waf configure --exp test
./waf --exp test
build/exp/test/test
