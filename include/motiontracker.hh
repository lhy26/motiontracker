#pragma once

/**
 * @mainpage
 *
 * This is the API reference of the Motion Tracker library and friends.
 * The general documentation is available in the README.markdown file.
 * The compilation instructions are also located there.
 */

#include <cv.h>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>
#include "utils.hh"

// Forward declarations
namespace cv {
	class VideoCapture;
}
namespace boost {
	class thread;
}

/**
 * @brief Class for storing camera parameters.
 *
 * It can also save/load them to/from an XML file.
 */
struct CalibrationParameters
{
	cv::Mat intrinsic_parameters; ///< Intrinsic parameters
	cv::Mat distortion_coeffs; ///< Distortion coefficients

	cv::Mat hues; ///< Hue values for object tracking
	cv::Mat dHues; ///< Hue thresholds for object tracking
	cv::Mat satval_l; ///< Saturation/Value low threshold for object tracking
	cv::Mat satval_h; ///< Saturation/Value high threshold for object tracking

	/// Dummy constructor
	CalibrationParameters();

	/// Constructor taking the parameters
	CalibrationParameters(cv::Mat ip, cv::Mat dc, cv::Mat hues, cv::Mat dHues, cv::Mat satval_l, cv::Mat satval_h);

	/**
	  * Saves the parameters to a configuration file.
	  * @param filename path to the file used for saving
	  */
	void saveToFile(std::string filename);

	/**
	  * Creates a new object by reading the parameters from a file.
	  * @param filename path to the file used for reading
	  * @return new object containing the read parameters
	  */
	static CalibrationParameters fromFile(std::string filename);
};



/**
 * @brief Opens a webcam and gets frames from it.
 */
class Webcam: public boost::noncopyable
{
public:
	/**
	 * Opens up a video capture device and starts recieving frames.
	 * @param device the capture deivce id, -1 means auto pick
	 */
	Webcam(int device = -1);

	/** Destructor closes device. */
	~Webcam();

	/** Thread runs here, don't call directly. */
	void operator()();

	/**
	 * Retrieve the latest frame.
	 * @param rhs the OpenCV matrix where the frame is stored
	 * @return a reference to the class it self for chaining
	 */
	Webcam& operator>>(cv::Mat& rhs);

	/**
	 * Get frame rate.
	 * @return frames per second
	 */
	int getFPS() const;

	/**
	  * Get the index number of the current frame.
	  * @return the index
	  */
	unsigned getFrameIndex() const;

	/**
	 * Check if the device is in zombie state.
	 * @return true if this class has no valid device
	 */
	bool isNull() { return m_capture == 0; }

private:
	boost::scoped_ptr<boost::thread> m_thread;
	boost::scoped_ptr<cv::VideoCapture> m_capture;
	mutable boost::mutex m_mutex;
	mutable boost::mutex m_fpsmutex;
	volatile bool m_quit;
	cv::Mat m_latestFrame;
	int m_fps;
	unsigned m_frameIndex;
};



/**
 * @brief Abstract class for receiving frames asynchronously from a webcam.
 */
struct WebcamListener: public boost::noncopyable
{
	/**
	 * Constructor launches the listener thread.
	 * @param webcam reference to a valid webcam for getting video
	 */
	WebcamListener(Webcam &webcam);

	/** Destructor kills the listener thread. */
	virtual ~WebcamListener();

	/** Thread runs here, don't call directly. */
	void operator()();

	/**
	 * This function is called every time a new frame is available.
	 * @param frame the frame in an OpenCV matrix
	 */
	virtual void frameEvent(const cv::Mat& frame) { (void)frame; }

	/**
	 * Returns a reference to the associated Webcam instance.
	 * @return the Webcam instance
	 */
	Webcam& getWebcam() { return m_webcam; }

private:

	Webcam &m_webcam; /// Reference to the Webcam object used for polling frames.
	boost::scoped_ptr<boost::thread> m_thread;; /// Receiver thread
	volatile bool m_quit; /// Flag telling the receiver to quit
};



/**
 * @brief Abstract class for calculating object orientation and position from webcam video.
 *
 * This is the base class for implementations that track certain kind of object.
 */
class MotionTracker: public WebcamListener
{
public:
	/**
	 * Constructor.
	 * @param webcam reference to a valid webcam for getting video
	 * @param camParams camera parameters from calibration
	 */
	MotionTracker(Webcam &webcam, const CalibrationParameters &camParams = CalibrationParameters());

	/** Virtual desctructor for derivatives. */
	virtual ~MotionTracker() {}

	/** Thread calls this, don't call directly. */
	virtual void frameEvent(const cv::Mat& frame) { (void)frame; }

	/**
	 * Returns the current orientation of the tracked object.
	 * @return the orientation in a vector
	 */
	cv::Vec3f getRotation() const;

	/**
	 * Returns the current orientation of the tracked object.
	 * @return the rotation matrix of the tracked object
	 */

	cv::Mat getRotationMatrix() const;

	/**
	 * Returns the current center position of the tracked object.
	 * @return the position in a vector
	 */
	cv::Vec3f getPosition() const;

	/**
	 * Get frame rate.
	 * @return frames per second
	 */
	int getFPS() const;

protected:
	mutable boost::mutex m_mutex; ///< Mutex for synchronization
	CalibrationParameters m_calibParams; ///< Camera parameters
	cv::Vec3f m_pos; ///< Position vector
	cv::Vec3f m_rot; ///< Rotation vector
	cv::Mat m_rotm; ///< Rotation matrix
	FPSCounter m_counter; ///< FPS counter
};



/**
 * @brief Calculates chessboard orientation and position from webcam video.
 */
class ChessboardTracker: public MotionTracker
{
public:
	/**
	 * Constructor.
	 * @param webcam reference to a valid webcam for getting video
	 * @param camParams camera parameters from calibration
	 */
	ChessboardTracker(Webcam &webcam, const CalibrationParameters &camParams);

	/** Thread calls this, don't call directly. */
	void frameEvent(const cv::Mat& frame);

private:
	// Test variables
	float m_boardScaleFactor; // Chessboard square edge length in units you want to use
	int m_boardH;
	int m_numCorners;
	cv::Size m_boardSize;
	vector<cv::Point3f> m_objectCorners;
	vector<cv::Point2f> m_corners;
};



/**
 * @brief Calculates the position of a unique color blob from a webcam video.
 */
class ColorTracker: public MotionTracker
{
public:
	/**
	 * Constructor.
	 * @param webcam reference to a valid webcam for getting video
	 * @param hue the approximate hue value of the color to be tracked
	 */
	ColorTracker(Webcam &webcam, int hue);

	/** Thread calls this, don't call directly. */
	void frameEvent(const cv::Mat& frame);

private:
	int m_hue;
};



/**
 * @brief Calculates the position and orientation of a cross with colored tips.
 */
class ColorCrossTracker: public MotionTracker
{
public:
	/**
	 * Constructor.
	 * @param webcam reference to a valid webcam for getting video
	 * @param calibParams the calibration parameters
	 * @param solver the method used: 1 = solvePnP, 2 = POSIT
	 */
	ColorCrossTracker(Webcam &webcam, const CalibrationParameters &calibParams, int solver = 2);

	/** Thread calls this, don't call directly. */
	void frameEvent(const cv::Mat& frame);

	/** Getter for image points vector. */
	std::vector<cv::Point2f> getImagePoints() const;

	/** Getter for projected points vector */
	std::vector<cv::Point2f> getProjectedPoints() const;

private:
	/// Calculates an image point for given hue, common to all solvers.
	/// @return true if point was found
	bool calculateImagePoint(const cv::Mat& frame, const int hue, const int dHue = 15, const int satval_low = 120, const int satval_high = 255);
	/// Solve pose using solvePnP algorithm.
	void solvePnP();
	/// Solve pose using POSIT algorithm.
	void solvePOSIT();

	CalibrationParameters m_calibParams;
	int m_solver;
	std::vector<cv::Point3f> m_objectPoints;
	std::vector<cv::Point2f> m_imagePoints;
	std::vector<cv::Point2f> m_savedImagePoints;

	// Posit stuff
	CvPOSITObject* m_positObject;
	std::vector<CvPoint2D32f> m_srcImagePoints;
	std::vector<CvPoint3D32f> m_modelPoints;

	std::vector<cv::Point2f> m_projectedPoints;
	std::vector<cv::Point2f> m_savedProjectedPoints;

};



/**
 * @brief Abstract class for receiving motion events asynchronously.
 */
struct MotionListener: public boost::noncopyable
{
	/**
	 * Constructor launches the listener thread.
	 */
	MotionListener(MotionTracker &motiontracker);
	
	/** Destructor kills the listener thread. */
	virtual ~MotionListener();

	/** Thread runs here, don't call directly. */
	void operator()();

	/** Get reference to the tracker object. */
	MotionTracker& getTracker() { return m_motionTracker; }

	/**
	 * This function is called every time new motion happens.
	 * @param pos the position vector of the tracked object
	 * @param rot the orientation vector of the tracked object
	 */
	virtual void motionEvent(cv::Vec3f pos, cv::Vec3f rot) { (void)pos; (void)rot; }

private:

	MotionTracker &m_motionTracker; /// Reference to MotionTracker object used for polling motion events.
	boost::scoped_ptr<boost::thread> m_thread;; /// Receiver thread
	volatile bool m_quit; /// Flag telling the receiver to quit
};


