#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/opencv.hpp>

#include <iostream>

// using namespace cv;
using namespace std;

static cv::Mat
visualize(cv::Mat input, cv::Mat faces, bool print_flag = false,
          double fps = -1, int thickness = 2)
{
  cv::Mat output = input.clone();

  if(fps > 0)
    {
      cv::putText(output, cv::format("FPS: %.2f", fps), cv::Point2i(0, 15),
                  cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0));
    }

  for(int i = 0; i < faces.rows; i++)
    {
      if(print_flag)
        {
          cout << "Face " << i << ", top-left coordinates: ("
               << faces.at<float>(i, 0) << ", " << faces.at<float>(i, 1)
               << "), "
               << "box width: " << faces.at<float>(i, 2)
               << ", box height: " << faces.at<float>(i, 3) << ", "
               << "score: " << faces.at<float>(i, 14) << "\n";
        }

      // Draw bounding box
      cv::rectangle(
          output,
          cv::Rect2i(int(faces.at<float>(i, 0)), int(faces.at<float>(i, 1)),
                     int(faces.at<float>(i, 2)), int(faces.at<float>(i, 3))),
          cv::Scalar(0, 255, 0), thickness);
      // Draw landmarks
      cv::circle(
          output,
          cv::Point2i(int(faces.at<float>(i, 4)), int(faces.at<float>(i, 5))),
          2, cv::Scalar(255, 0, 0), thickness);
      cv::circle(
          output,
          cv::Point2i(int(faces.at<float>(i, 6)), int(faces.at<float>(i, 7))),
          2, cv::Scalar(0, 0, 255), thickness);
      cv::circle(
          output,
          cv::Point2i(int(faces.at<float>(i, 8)), int(faces.at<float>(i, 9))),
          2, cv::Scalar(0, 255, 0), thickness);
      cv::circle(output,
                 cv::Point2i(int(faces.at<float>(i, 10)),
                             int(faces.at<float>(i, 11))),
                 2, cv::Scalar(255, 0, 255), thickness);
      cv::circle(output,
                 cv::Point2i(int(faces.at<float>(i, 12)),
                             int(faces.at<float>(i, 13))),
                 2, cv::Scalar(0, 255, 255), thickness);
      // Put score
      cv::putText(output, cv::format("%.4f", faces.at<float>(i, 14)),
                  cv::Point2i(int(faces.at<float>(i, 0)),
                              int(faces.at<float>(i, 1)) + 15),
                  cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0));
    }
  return output;
}

int
main(int argc, char **argv)
{
  // clang-format off
  cv::CommandLineParser parser(argc, argv,
          "{help  h           |            | Print this message.}"
          "{input_image       |            | Path to the input image.}"
          "{input_video       |            | Path to the input video.}"
          "{scale sc          | 1.0        | Scale factor used to resize input video frames}"
          "{backend_id        | 0          | Backend to run on. 0: default, 1: Halide, 2: Intel's Inference Engine, 3: OpenCV, 4: VKCOM, 5: CUDA}"
          "{target_id         | 0          | Target to run on. 0: CPU, 1: OpenCL, 2: OpenCL FP16, 3: Myriad, 4: Vulkan, 5: FPGA, 6: CUDA, 7: CUDA FP16, 8: HDDL}"
          "{model m           | yunet.onnx | Path to the model. Download yunet.onnx in https://github.com/ShiqiYu/libfacedetection.train/tree/master/tasks/task1/onnx.}"
          "{score_threshold   | 0.7        | Filter out faces of score < score_threshold.}"
          "{nms_threshold     | 0.3        | Suppress bounding boxes of iou >= nms_threshold.}"
          "{top_k             | 5000       | Keep top_k bounding boxes before NMS.}"
          );
  // clang-format on

  if(parser.has("help"))
    {
      parser.printMessage();
      return -1;
    }

  cv::String modelPath = parser.get<cv::String>("model");
  float scale = parser.get<float>("scale");
  int backendId = parser.get<int>("backend_id");
  int targetId = parser.get<int>("target_id");

  float scoreThreshold = parser.get<float>("score_threshold");
  float nmsThreshold = parser.get<float>("nms_threshold");
  int topK = parser.get<int>("top_k");

  // Initialize FaceDetectorYN
  cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
      modelPath, "", cv::Size(320, 320), scoreThreshold, nmsThreshold, topK,
      backendId, targetId);

  // If input is an image
  if(parser.has("input_image"))
    {
      cv::String input = parser.get<cv::String>("input_image");
      cv::Mat image = cv::imread(input);
      if(image.empty())
        {
          std::cerr << "Cannot read image: " << input << std::endl;
          return 2;
        }

      int imageWidth = int(image.cols * scale);
      int imageHeight = int(image.rows * scale);
      resize(image, image, cv::Size(imageWidth, imageHeight));
      detector->setInputSize(cv::Size(imageWidth, imageHeight));

      while(cv::waitKey(1) < 0) // Press any key to exit
        {
          cv::Mat faces;

          cv::TickMeter tm;
          tm.start();
          detector->detect(image, faces);
          tm.stop();

          printf("time = %gms\n", tm.getTimeMilli());

          cv::Mat vis_image = visualize(image, faces, false, tm.getFPS());

          imshow("libfacedetection demo", vis_image);

          tm.reset();
        }
    }
  else if(parser.has("input_video"))
    {
      cv::VideoCapture cap;
      cv::String input = parser.get<cv::String>("input_video");
      cap.open(input, cv::CAP_ANY);
      int frameWidth = int(cap.get(cv::CAP_PROP_FRAME_WIDTH) * scale);
      int frameHeight = int(cap.get(cv::CAP_PROP_FRAME_HEIGHT) * scale);
      detector->setInputSize(cv::Size(frameWidth, frameHeight));

      cv::Mat frame;
      cv::TickMeter tm;
      while(cv::waitKey(1) < 0) // Press any key to exit
        {
          if(!cap.read(frame))
            {
              cerr << "No frames grabbed!\n";
              break;
            }
          resize(frame, frame, cv::Size(frameWidth, frameHeight));

          cv::Mat faces;
          tm.start();
          detector->detect(frame, faces);
          tm.stop();

          cv::Mat vis_frame = visualize(frame, faces, false, tm.getFPS());

          imshow("libfacedetection demo", vis_frame);

          tm.reset();
        }
    }
}
