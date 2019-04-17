/*
MIT License

Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// header file "annmodule.h" generated by nnir_to_openvx.py
#include "annmodule.h"

// header file to include OpenVX Modules
#include <vx_ext_amd.h>
#include <vx_amd_nn.h>

// c/c++ includes
#include <chrono>

// Use OpenCV Functions
#if ENABLE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv/cv.h>
#include <opencv/highgui.h>

// Header file details in include folder
#define CVUI_IMPLEMENTATION
#include "cvui.h"

using namespace cv;
#endif

// Name Output Display Windows
#define MIVisionX_LEGEND    "MIVisionX Image Classification"
#define MIVisionX_DISPLAY   "MIVisionX Image Classification - Display"


// probability track bar
const int threshold_slider_max = 100;
int threshold_slider;
double thresholdValue = 0.5;
void threshold_on_trackbar( int, void* ){
    thresholdValue = (double) threshold_slider/threshold_slider_max ;
    return;
}

bool runModel = false;
float modelTime_g;

// create legend image for the app
void createLegendImage(std::string modelName)
{
    // create display legend image
    int fontFace = CV_FONT_HERSHEY_DUPLEX;
    double fontScale = 0.75;
    int thickness = 1.3;
    cv::Size legendGeometry = cv::Size(625, (7 * 40) + 40);
    Mat legend = Mat::zeros(legendGeometry,CV_8UC3);
    Rect roi = Rect(0,0,625,(7 * 40) + 40);
    legend(roi).setTo(cv::Scalar(128,128,128));
    int l = 0, model = 0;
    int red, green, blue;
    std::string className;
    std::string bufferName;
    char buffer [50];

    // add headers
    bufferName = MIVisionX_LEGEND;
    putText(legend, bufferName, Point(20, (l * 40) + 30), fontFace, 1.2, cv::Scalar(66,13,9), thickness,5);
    l++;
    l++;
    bufferName = "Model";
    putText(legend, bufferName, Point(100, (l * 40) + 30), fontFace, 1, Scalar::all(0), thickness,5);
    bufferName = "ms/frame";
    putText(legend, bufferName, Point(300, (l * 40) + 30), fontFace, 1, Scalar::all(0), thickness,5);
    bufferName = "Color";
    putText(legend, bufferName, Point(525, (l * 40) + 30), fontFace, 1, Scalar::all(0), thickness,5);
    l++;
    
    // add legend item
    thickness = 1;    
    red = 255; green = 0; blue = 0;
    className = modelName;
    sprintf (buffer, " %.2f ", modelTime_g );
    cvui::checkbox(legend, 30, (l * 40) + 15,"", &runModel);
    putText(legend, className, Point(80, (l * 40) + 30), fontFace, fontScale, Scalar::all(0), thickness,3);
    putText(legend, buffer, Point(320, (l * 40) + 30), fontFace, fontScale, Scalar::all(0), thickness,3);
    rectangle(legend, Point(550, (l * 40)) , Point(575, (l * 40) + 40), Scalar(blue,green,red),-1);
    l++;
    l++;

    // Model Confidence Threshold
    bufferName = "Confidence";
    putText(legend, bufferName, Point(250, (l * 40) + 30), fontFace, 1, Scalar::all(0), thickness,5);
    l++;
    cvui::trackbar(legend, 100, (l * 40)+10, 450, &threshold_slider, 0, 100);
    cvui::update();

    cv::imshow(MIVisionX_LEGEND, legend);
    thresholdValue = (double) threshold_slider/threshold_slider_max ;
}


#define ERROR_CHECK_OBJECT(obj) { vx_status status = vxGetStatus((vx_reference)(obj)); if(status != VX_SUCCESS) { vxAddLogEntry((vx_reference)context, status     , "ERROR: failed with status = (%d) at " __FILE__ "#%d\n", status, __LINE__); return status; } }
#define ERROR_CHECK_STATUS(call) { vx_status status = (call); if(status != VX_SUCCESS) { printf("ERROR: failed with status = (%d) at " __FILE__ "#%d\n", status, __LINE__); return -1; } }

static void VX_CALLBACK log_callback(vx_context context, vx_reference ref, vx_status status, const vx_char string[])
{
    size_t len = strlen(string);
    if (len > 0) {
        printf("%s", string);
        if (string[len - 1] != '\n')
            printf("\n");
        fflush(stdout);
    }
}

inline int64_t clockCounter()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

inline int64_t clockFrequency()
{
    return std::chrono::high_resolution_clock::period::den / std::chrono::high_resolution_clock::period::num;
}

// usage guide
static void show_usage()
{ 
    printf(
            "\n"
            "Usage: ./classifier \n"
            //"--video <video file>/--capture <0>/--image <image file>        [required]\n"
            "--video <video file>/--capture <0>         [required]\n"
            "--model_weights   <model-weights.bin>      [required]\n"    
            "--label           <label text>             [required]\n"
            "--model_name      <model name>             [optional -- default:NN_ModelName]\n" 
            "--model_inputs    <3,224,224>              [c,h,w - optional -- default:3,224,224]\n"
            "--model_outputs   <1000>                   [num output classes - optional -- default:1000]\n\n" 
            "[options] --help/--h\n"   
            "\n"
        );
}

int main(int argc, const char ** argv)
{
    // check command-line usage   
    std::string modelWeights_str = "empty";
    std::string videoFile = "empty";
    std::string imageFile = "empty";
    std::string labelFileName = "empty";
    std::string modelInputs = "empty";
    std::string NN_ModelName = "NN-Model";
    std::string labelText[1000];

    int captureID = -1;
    bool captureFromVideo = false;
    bool imageFileInput = false;
    int input_c = 3, input_h = 224, input_w = 224;
    int modelOutput = 1000;
    
    int parameter = 0;
    vx_status status = 0;

    for(int arg = 1; arg < argc; arg++)
    {
        if (!strcasecmp(argv[arg], "--help") || !strcasecmp(argv[arg], "--H") || !strcasecmp(argv[arg], "--h"))
        {
            show_usage();
            exit(status);
        }
        else if (!strcasecmp(argv[arg], "--model_weights"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing model weights .bin file location on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            modelWeights_str = (argv[arg]);
            parameter++;
        }
        else if (!strcasecmp(argv[arg], "--label"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing label.txt file on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            labelFileName = (argv[arg]);
            std::string line;
	        std::ifstream out(labelFileName);
	        int lineNum = 0;
	        while(getline(out, line)) {
	            labelText[lineNum] = line;
	            lineNum++;
	        }
	        out.close();
	        parameter++;
        }
        else if (!strcasecmp(argv[arg], "--video"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing video file on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            videoFile = (argv[arg]);
            captureFromVideo = true;
            parameter++;
        }
        else if (!strcasecmp(argv[arg], "--image"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing image file on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            imageFile = (argv[arg]);
            imageFileInput = true;
            parameter++;
        }
        else if (!strcasecmp(argv[arg], "--capture"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing camera source on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            captureID = atoi(argv[arg]);
            parameter++;
        }
        else if (!strcasecmp(argv[arg], "--model_name"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing model name on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            NN_ModelName = (argv[arg]);
        }
        else if (!strcasecmp(argv[arg], "--model_inputs"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing model inputs on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            modelInputs = (argv[arg]);
            
            std::vector<int> vect;
            std::stringstream ss(modelInputs);
            int i;
            while (ss >> i)
            {
                vect.push_back(i);
                if (ss.peek() == ',')
                    ss.ignore();
            }
            input_c = vect.at(0);
            input_h = vect.at(1);
            input_w = vect.at(2);
        }
        else if (!strcasecmp(argv[arg], "--model_outputs"))
        {
            if ((arg + 1) == argc)
            {
                printf("\n\nERROR: missing model outputs on command-line (see help for details)\n\n\n");
                show_usage();
                status = -1;
                exit(status);
            }
            arg++;
            modelOutput = atoi(argv[arg]);
        }
    }

    if (parameter < 3)
    {
        printf("\nERROR: missing parameters in command-line.\n");
        show_usage();
        status = -1;
        exit(status);
    }

    // create context, input, output, and graph
    vxRegisterLogCallback(NULL, log_callback, vx_false_e);
    vx_context context = vxCreateContext();
    status = vxGetStatus((vx_reference)context);
    if(status) {
        printf("ERROR: vxCreateContext() failed\n");
        return -1;
    }
    vxRegisterLogCallback(context, log_callback, vx_false_e);

    // creation of graphs
    vx_graph model_graph = vxCreateGraph(context);
    status = vxGetStatus((vx_reference)model_graph);
    if(status) {
        printf("ERROR: vxCreateGraph(...) failed (%d)\n", status);
        return -1;
    }
    
    // create and initialize input tensor data
    vx_size dims_input_data[4] = { vx_size(input_w), vx_size(input_h), vx_size(input_c), 1 };

    // create data for different sizes
    vx_tensor input_data_tensor = vxCreateTensor(context, 4, dims_input_data, VX_TYPE_FLOAT32, 0);
    if(vxGetStatus((vx_reference)input_data_tensor)) {
        printf("ERROR: vxCreateTensor() failed for data\n");
        return -1;
    }

    // create output tensor prob
    vx_size dims_prob_data[4] = { 1, 1, vx_size(modelOutput), 1 };
    vx_tensor output_prob_tensor = vxCreateTensor(context, 4, dims_prob_data, VX_TYPE_FLOAT32, 0);
    if(vxGetStatus((vx_reference)output_prob_tensor)) {
        printf("ERROR: vxCreateTensor() failed for prob\n");
        return -1;
    }
    

    // build graph using annmodule
    int64_t freq = clockFrequency(), t0, t1;
    char modelWeights[1024];
    strcpy(modelWeights, modelWeights_str.c_str());


    // create model graph
    t0 = clockCounter();
    if(modelWeights_str != "empty"){
    	status = annAddToGraph(model_graph, input_data_tensor, output_prob_tensor, modelWeights);
	    if(status) {
	        printf("ERROR: Model annAddToGraph() failed (%d)\n", status);
	        return -1;
	    }
	    status = vxVerifyGraph(model_graph);
	    if(status) {
	        printf("ERROR: Model vxVerifyGraph(...) failed (%d)\n", status);
	        return -1;
	    }
        runModel = true;
    } 
    t1 = clockCounter();
    printf("OK: graph initialization with annAddToGraph() took %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

    // test process graph 
    t0 = clockCounter();
    status = vxProcessGraph(model_graph);
    if(status != VX_SUCCESS) {
        printf("ERROR: vxProcessGraph() failed (%d)\n", status);
        return -1;
    }
    t1 = clockCounter();
    printf("OK: vxProcessGraph() took %.3f msec (1st iteration)\n", (float)(t1-t0)*1000.0f/(float)freq);

    // avg model time on the system
    int N = 100;
    float modelTime;
    if(modelWeights_str != "empty")
    {
        t0 = clockCounter();
        for(int i = 0; i < N; i++) {
            status = vxProcessGraph(model_graph);
            if(status != VX_SUCCESS)
                break;
        }
        t1 = clockCounter();
        modelTime = (float)(t1-t0)*1000.0f/(float)freq/(float)N;
        printf("OK: NN Model took %.3f msec (average over %d iterations)\n", (float)(t1-t0)*1000.0f/(float)freq/(float)N, N);
    }

    /***** OPENCV Additions *****/

    // create display windows
    cv::namedWindow(MIVisionX_LEGEND);
    cvui::init(MIVisionX_LEGEND);
    cv::namedWindow(MIVisionX_DISPLAY, cv::WINDOW_GUI_EXPANDED);

    // probability track bar threshold default
    threshold_slider = 50;
    
    // create display legend image
    modelTime_g = modelTime;
    createLegendImage(NN_ModelName);

    // define variables for run
    cv::Mat frame;
    int outputImgWidth = 1080, outputImgHeight = 720;
    float threshold = 0.01;
    cv::Size output_geometry = cv::Size(outputImgWidth, outputImgHeight);
    Mat inputDisplay, outputDisplay;  

    cv::Mat inputFrame_299x299, inputFrame_data_resized;
    int fontFace = CV_FONT_HERSHEY_DUPLEX;
    double fontScale = 1;
    int thickness = 1.5;
    float *outputBuffer[1];
    for(int models = 0; models < 1; models++){
        outputBuffer[models] = new float[modelOutput];
    }

    int loopSeg = 1;

    while(argc && loopSeg)
    {
        VideoCapture cap;
        if (captureFromVideo) {
            cap.open(videoFile);
            if(!cap.isOpened()) {
                std::cout << "Unable to open the video: " << videoFile << std::endl;
                return 0;
            }
        }
        else {
            cap.open(captureID);
            if(!cap.isOpened()) {
                std::cout << "Unable to open the camera feed: " << captureID << std::endl;
                return 0;
            }
        }

        int frameCount = 0;
        float msFrame = 0, fpsAvg = 0, frameMsecs = 0;
        for(;;)
        {
            msFrame = 0;
            // capture image frame
            t0 = clockCounter();
            cap >> frame;
            if( frame.empty() ) break; // end of video stream
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("\n\nLIVE: OpenCV Frame Capture Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

            // preprocess image frame
            t0 = clockCounter();
            cv::resize(frame, inputFrame_data_resized, cv::Size(input_h,input_w));
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: OpenCV Frame Resize Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

            // Copy Image frame into the input tensor
            t0 = clockCounter();
            vx_enum usage = VX_WRITE_ONLY;
            vx_enum data_type = VX_TYPE_FLOAT32;
            vx_size num_of_dims = 4, dims[4] = { 1, 1, 1, 1 }, stride[4];
            vx_map_id map_id;
            float * ptr;
            vx_size count;

            // copy - 224x224 image tensor
            if(runModel)
            {
                vxQueryTensor(input_data_tensor, VX_TENSOR_DATA_TYPE, &data_type, sizeof(data_type));
                vxQueryTensor(input_data_tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(num_of_dims));
                vxQueryTensor(input_data_tensor, VX_TENSOR_DIMS, &dims, sizeof(dims[0])*num_of_dims);
                if(data_type != VX_TYPE_FLOAT32) {
                    std::cerr << "ERROR: copyTensor() supports only VX_TYPE_FLOAT32: invalid for " <<  std::endl;
                    return -1;
                }
                count = dims[0] * dims[1] * dims[2] * dims[3];
                vx_status status = vxMapTensorPatch(input_data_tensor, num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, usage, VX_MEMORY_TYPE_HOST, 0);
                if(status) {
                    std::cerr << "ERROR: vxMapTensorPatch() failed for " <<  std::endl;
                    return -1;
                }
                Mat srcImg;
                for(size_t n = 0; n < dims[3]; n++) {
                    srcImg = inputFrame_data_resized;
                    for(vx_size y = 0; y < dims[1]; y++) {
                        unsigned char * src = srcImg.data + y*dims[0]*3;
                        float * dstR = ptr + ((n * stride[3] + y * stride[1]) >> 2);
                        float * dstG = dstR + (stride[2] >> 2);
                        float * dstB = dstG + (stride[2] >> 2);
                        for(vx_size x = 0; x < dims[0]; x++, src += 3) {
                            if(input_w != 299){
                            *dstR++ = src[2];
                            *dstG++ = src[1];
                            *dstB++ = src[0];
                            }
                            else{
                            *dstR++ = (src[2] * 0.007843137) - 1;
                            *dstG++ = (src[1] * 0.007843137) - 1;
                            *dstB++ = (src[0] * 0.007843137) - 1;
                            }

                        }
                    }
                }
                status = vxUnmapTensorPatch(input_data_tensor, map_id);
                if(status) {
                    std::cerr << "ERROR: vxUnmapTensorPatch() failed for " <<  std::endl;
                    return -1;
                }
            }
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: Convert Image to Tensor Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);
   
            // process graph for the input           
            if(runModel)
            {
                t0 = clockCounter();
                status = vxProcessGraph(model_graph);
                if(status != VX_SUCCESS) break;
                t1 = clockCounter();
                modelTime_g = (float)(t1-t0)*1000.0f/(float)freq;
                msFrame += (float)(t1-t0)*1000.0f/(float)freq;
                //printf("LIVE: Process Resnet50 Classification Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);
            }

            // copy output data into local buffer
            t0 = clockCounter();
            usage = VX_READ_ONLY;
            if(runModel)
            {
                vxQueryTensor(output_prob_tensor, VX_TENSOR_DATA_TYPE, &data_type, sizeof(data_type));
                vxQueryTensor(output_prob_tensor, VX_TENSOR_NUMBER_OF_DIMS, &num_of_dims, sizeof(num_of_dims));
                vxQueryTensor(output_prob_tensor, VX_TENSOR_DIMS, &dims, sizeof(dims[0])*num_of_dims);
                if(data_type != VX_TYPE_FLOAT32) {
                    std::cerr << "ERROR: copyTensor() supports only VX_TYPE_FLOAT32: invalid for "  << std::endl;
                    return -1;
                }
                count = dims[0] * dims[1] * dims[2] * dims[3];
                status = vxMapTensorPatch(output_prob_tensor, num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, usage, VX_MEMORY_TYPE_HOST, 0);
                if(status) {
                    std::cerr << "ERROR: vxMapTensorPatch() failed for "  << std::endl;
                    return -1;
                }
                memcpy(outputBuffer[0], ptr, (count*sizeof(float)));
                status = vxUnmapTensorPatch(output_prob_tensor, map_id);
                if(status) {
                    std::cerr << "ERROR: vxUnmapTensorPatch() failed for "  << std::endl;
                    return -1;
                }
            }
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: Copy probability Output Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

            // process probabilty
            t0 = clockCounter();
            threshold = (float)thresholdValue;
            const int N = modelOutput;
            int resnetID;
            if(runModel)
            {
                resnetID = std::distance(outputBuffer[0], std::max_element(outputBuffer[0], outputBuffer[0] + N));
            }
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: Get Classification ID Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

            // Write Output on Image
            t0 = clockCounter();
            cv::resize(frame, outputDisplay, cv::Size(outputImgWidth,outputImgHeight));
            int l = 1;
            std::string modelName = NN_ModelName + " -- ";
            std::string modelText = "Unclassified";
            if(outputBuffer[0][resnetID] >= threshold){ modelText = labelText[resnetID]; }

            modelName = modelName + modelText;
            int red, green, blue;
            if(runModel && modelWeights_str != "empty")
            {
                red = 255; green = 0; blue = 0;
                putText(outputDisplay, modelName, Point(20, (l * 40) + 30), fontFace, fontScale, Scalar(blue,green,red), thickness,8);
                l++;
            }
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: Resize and write on Output Image Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);
   
            // display img time
            t0 = clockCounter();
            cv::imshow(MIVisionX_DISPLAY, outputDisplay);
            createLegendImage(NN_ModelName);
            t1 = clockCounter();
            msFrame += (float)(t1-t0)*1000.0f/(float)freq;
            //printf("LIVE: Output Image Display Time -- %.3f msec\n", (float)(t1-t0)*1000.0f/(float)freq);

            // calculate FPS
            //printf("LIVE: msec for frame -- %.3f msec\n", (float)msFrame);
            frameMsecs += msFrame;
            if(frameCount && frameCount%10 == 0){
                printf("FPS LIVE: Avg FPS -- %d\n", (int)((ceil)(1000/(frameMsecs/10))));
                frameMsecs = 0;
            }

            // wait to close live inference application
            if( waitKey(2) == 27 ){ loopSeg = 0; break; } // stop capturing by pressing ESC
            else if( waitKey(2) == 82 ){ break; } // for restart pressing R

            frameCount++;
        }
    }

    // release resources
    for(int models = 0; models < 1; models++){
        delete outputBuffer[models];
    }
    // release input data
    ERROR_CHECK_STATUS(vxReleaseTensor(&input_data_tensor));
    // release output data
    ERROR_CHECK_STATUS(vxReleaseTensor(&output_prob_tensor));
    // release graphs
    ERROR_CHECK_STATUS(vxReleaseGraph(&model_graph));
    // release context
    ERROR_CHECK_STATUS(vxReleaseContext(&context));

    printf("OK: MIVisionX Classifier Successful\n");
    return 0;
}