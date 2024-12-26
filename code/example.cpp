#include <iostream>
#include <opencv2/opencv.hpp>

int main() {
    cv::Mat image = cv::Mat::zeros(512, 512, CV_8UC3);
    cv::circle(image, cv::Point(256, 256), 40, cv::Scalar(0, 255, 0), -1);
    cv::imshow("Test Window", image);
    cv::waitKey(0);
    return 0;
}
