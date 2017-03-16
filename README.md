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

## Technical details


1. Streamed input.
Training on giant datasets can be hard because they don't always fit on your hard drive. I am talking about datasets that are 1 terabyte or even more. People would often compress these datasets into a zip/gzip/7zip file for more efficient storage, but it still needs to be unzipped for training.
However with tealtree, you can simply say something like: 
gunzip training_data.csv.gz | tealtree --train ...
This also means that you can read the training data directly from various cloud storages, like Amaon S3 or Azure, without the need to download the dataset file locally first.

2. Training data transposition.
A typical dataset format, be it in csv or svm format, arranges data by document. However for gradient boosting it is more convenient to store data by the feature. So tealtree transposes data internally. That also allows to handle different features with different algorithms, for example, tealtree distinguishes dense features from sparse ones and uses different optimization techniques.

3. Bucketizing feature values.
Many gradient boosting frameworks consider all feature values to be floating point numbers. This is inefficient, since the only way these values are used during the training is that they are compared to a set of threshold or hurdle values.
Tealtree precomputes these threshold values for every feature when loading the dataset. These thesholds break the set of feature values into N 
 (sort of) even intervals or buckets. So now we can replace the floating point feature values with a bucket id, which often times fits into 1 byte, making things more efficient.

4. Feature encoding.
Features are different. Some features are binary, others can have millions of distinct values.
For binary features tealtree uses 1 bit per value encoding. f a feature has 4, 16 or 256 distinct values, tealtree would use 2-bit, 4-bit or 8-bit per value encodings. If a feature has millions of distinct values, then tealtree can bucketize it into up to 65536 buckets and use 2-bytes per value encoding.
This encoding is as memory efficient as it gets, providing even more speed boost.

5. Sparse features.
Tealtree handles Sparse features efficiently. For a sparse feature we need to store the explicit (non-zero) values, as well as their indices. Tealtree stores the explicit values in the best encoding (according to the number of distinct feature values). As for the indices, tealtree only stores the deltas between the consecutive values using variable length integer (varint) encoding.

6. Fast sparse features.
Let's get a little more technical. From the computational point of view the most frequently repeated operation in GBDT is computing a histogram on a leaf. When using a sparse feature the problem is that we can only iterate over all the explicit feature values from beginning to the end, since the indices are stored as deltas. In particular, for a tinyt leaf we still have to iterate over all the sparse features. Dense features don't have this problem, since we can easily read i-th value from an array no matter how many bits per value it is encoded.
The key idea behind fast sparse features is to partition itself into a number of segments, where each segmtn corresponds to a particular leaf in the current tree. That would involve rearranging data every time a leaf is split in two, but the upside is that the histogram computation is much faster, especially for small leaves. According to my measurements this approach pays off for large trees, 100 leaves or more. It can often show 2x speedup over the (non-fast) sparse features.

7. Objectives.
Tealtree supports regression, binary classification and ranking using Lambda MART.

8. Multi-threading.
Tealtree supports multithreading out of the box.

9. Multi-node (parallel) computation
I designed tealtree to be scalable. I was planning to implement multi-node computation using an RPC library like Google protobuf or Facebook thrift. However this feature is not implemented yet.

## Questions?
Feedback, comments, feature requests, etc?
Email me at <logan42@linuxmail.org>.
