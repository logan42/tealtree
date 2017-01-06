# TealTree

##Introduction
TealTree is a highly optimized implementation of a [gradient boosting decision tree](https://en.wikipedia.org/wiki/Gradient_boosting) (GBDT) algorithm.
TealTree currently supports regression, binary classification and ranking (using LambdaMART) as training objectives.
TealTree is faster than its competitors. 
In particular, TealTree is typically 4-5 times faster than xgboost.

## Installation

### Debian/Ubuntu Linux
```
git clone https://github.com/logan42/tealtree
cd tealtree
./build_linux.sh
```

### OSX
```
git clone https://github.com/logan42/tealtree
cd tealtree
./build_mac.sh
```

### Windows
```
git clone https://github.com/logan42/tealtree
```

And, finally, open "VS2015 command line tools" window", navigate to tealtree directory, and execute:
```
build_windows.cmd
```

## Examples
You can find some examples of how to use TealTree in [examples](examples) directory.

## Questions?
Feedback, comments, feature requests, etc?
Email me at <logan42@linuxmail.org>.
