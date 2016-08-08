# TealTree

##Introduction
TealTree is a highly optimized implementation of a [gradient boosting decision tree](https://en.wikipedia.org/wiki/Gradient_boosting) (GBDT) algorithm.
TealTree currently supports regression, binary classification and ranking (using LambdaMART) as training objectives.
TealTree is faster than its competitors. 
In particular, TealTree is typically 2-3 times faster than xgboost.

## Installation

### Debian/Ubuntu
TealTree requires [Boost](http://www.boost.org/) version 1.59 or above.
Check the version of Boost available on your system:
```
apt-cache policy libboost-all-dev
```
If this command displays version 1.59 or above, then install Boost from the repository:
```
sudo apt-get install libboost-all-dev
```
Otherwise, download and compile Boost from sources. 
```
curl -L 'http://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2/download' | tar jxf -
cd boost_1_61_0
./bootstrap.sh --prefix=/usr/local
sudo ./b2 install 
```
Now we are ready to compile TealTree:
```
git clone https://github.com/logan42/tealtree
cd tealtree
./build_linux.sh
```

### OSX
TealTree requires [Boost](http://www.boost.org/) version 1.59 or above.
Install boost using your favourite ports manager. 
If you are using (MacPorts)[https://www.macports.or], then execute:
```
sudo port install boost
```
Now we are ready to compile TealTree:
```
git clone https://github.com/logan42/tealtree
cd tealtree
./build_mac.sh
```

### Windows
TealTree has been tested on Windows 10 and Visual Studio 2015.

Please download and install [Boost](http://www.boost.org/) library version 1.59 or above.
You can refer to the [build instructions](http://www.boost.org/doc/libs/1_61_0/more/getting_started/windows.html).

Next, clone the git repository:
```
git clone https://github.com/logan42/tealtree
```

Open `tealtree/build_windows.cmd` file and edit the include and linker libraries paths to point to your Boost library directory.

And, finally, open "VS2015 command line tools" window", navigate to tealtree directory, and execute:
```
build_windows.cmd
```

## Examples
You can find some examples of how to use TealTree in [examples](examples) directory.

## Questions?
Feedback, comments, feature requests, etc?
Email me at <logan42@linuxmail.org>.
