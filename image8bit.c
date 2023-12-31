/// image8bit - A simple image processing module.
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// João Manuel Rodrigues <jmr@ua.pt>
/// 2013, 2023

// Student authors (fill in below):
// NMec:  Name:
// 76639  Filipe Tavares de Sousa
// 110509 Rui Albuquerque
// 
// Date:
// 26/11/2023

#include "image8bit.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "instrumentation.h"

// The data structure
//
// An image is stored in a structure containing 3 fields:
// Two integers store the image width and height.
// The other field is a pointer to an array that stores the 8-bit gray
// level of each pixel in the image.  The pixel array is one-dimensional
// and corresponds to a "raster scan" of the image from left to right,
// top to bottom.
// For example, in a 100-pixel wide image (img->width == 100),
//   pixel position (x,y) = (33,0) is stored in img->pixel[33];
//   pixel position (x,y) = (22,1) is stored in img->pixel[122].
// 
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// Maximum value you can store in a pixel (maximum maxval accepted)
const uint8 PixMax = 255;

void * memcpy(void* , void*, size_t);

// Internal structure for storing 8-bit graymap images
struct image {
  int width;
  int height;
  int maxval;   // maximum gray value (pixels with maxval are pure WHITE)
  uint8* pixel; // pixel data (a raster scan)
};


// This module follows "design-by-contract" principles.
// Read `Design-by-Contract.md` for more details.

/// Error handling functions

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
// 
// When one of these functions fails, it signals this by returning an error
// value such as NULL or 0 (see function documentation), and sets an internal
// variable (errCause) to a string indicating the failure cause.
// The errno global variable thoroughly used in the standard library is
// carefully preserved and propagated, and clients can use it together with
// the ImageErrMsg() function to produce informative error messages.
// The use of the GNU standard library error() function is recommended for
// this purpose.
//
// Additional information:  man 3 errno;  man 3 error;

// Variable to preserve errno temporarily
static int errsave = 0;

// Error cause
static char* errCause;

/// Error cause.
/// After some other module function fails (and returns an error code),
/// calling this function retrieves an appropriate message describing the
/// failure cause.  This may be used together with global variable errno
/// to produce informative error messages (using error(), for instance).
///
/// After a successful operation, the result is not garanteed (it might be
/// the previous error cause).  It is not meant to be used in that situation!
char* ImageErrMsg() { ///
  return errCause;
}


// Defensive programming aids
//
// Proper defensive programming in C, which lacks an exception mechanism,
// generally leads to possibly long chains of function calls, error checking,
// cleanup code, and return statements:
//   if ( funA(x) == errorA ) { return errorX; }
//   if ( funB(x) == errorB ) { cleanupForA(); return errorY; }
//   if ( funC(x) == errorC ) { cleanupForB(); cleanupForA(); return errorZ; }
//
// Understanding such chains is difficult, and writing them is boring, messy
// and error-prone.  Programmers tend to overlook the intricate details,
// and end up producing unsafe and sometimes incorrect programs.
//
// In this module, we try to deal with these chains using a somewhat
// unorthodox technique.  It resorts to a very simple internal function
// (check) that is used to wrap the function calls and error tests, and chain
// them into a long Boolean expression that reflects the success of the entire
// operation:
//   success = 
//   check( funA(x) != error , "MsgFailA" ) &&
//   check( funB(x) != error , "MsgFailB" ) &&
//   check( funC(x) != error , "MsgFailC" ) ;
//   if (!success) {
//     conditionalCleanupCode();
//   }
//   return success;
// 
// When a function fails, the chain is interrupted, thanks to the
// short-circuit && operator, and execution jumps to the cleanup code.
// Meanwhile, check() set errCause to an appropriate message.
// 
// This technique has some legibility issues and is not always applicable,
// but it is quite concise, and concentrates cleanup code in a single place.
// 
// See example utilization in ImageLoad and ImageSave.
//
// (You are not required to use this in your code!)


// Check a condition and set errCause to failmsg in case of failure.
// This may be used to chain a sequence of operations and verify its success.
// Propagates the condition.
// Preserves global errno!
static int check(int condition, const char* failmsg) {
  errCause = (char*)(condition ? "" : failmsg);
  return condition;
}


/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) { ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
  InstrName[1] = "countlocate";
  InstrName[2] = "countblur";
  InstrName[3] = "sumblur";
  
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Add more macros here...
#define CountLocate InstrCount[1]
#define CountBlur InstrCount[2]
#define SumBlur InstrCount[3]

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!


/// Image management functions

/// Create a new black image.
///   width, height : the dimensions of the new image.
///   maxval: the maximum gray level (corresponding to white).
/// Requires: width and height must be non-negative, maxval > 0.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCreate(int width, int height, uint8 maxval) { ///
  assert (width >= 0);
  assert (height >= 0);
  assert (0 < maxval && maxval <= PixMax);
  //Allocates memory for a new Image
  Image newImg = (Image)malloc(sizeof(struct image));
  //Checks if it is possible to allocate memory
  //If it is not possible, shows error image and returns NULL
  if (!check (newImg != NULL, "Memory couldn't be allocated for new image!")) {
	  return NULL;
  }

  newImg->width = width;
  newImg->height = height;
  newImg->maxval = maxval;
  //Calculates the number of pixels necessary
  size_t pixelSize = width * height * sizeof(uint8);
  newImg->pixel = (uint8*)malloc(pixelSize);
  //Verifies if there's pixels values in the new image created
  //If it doesn't verify, shows error message, free the space previously allocated and returns NULL
  if (!check(newImg->pixel != NULL, "No pixel in image!")) {
	  free(newImg);
	  return NULL;
  }
  //Creates a black Image by altering every pixel in the image to a pixel with the value 0 (black)
  for (int i = 0; i < width * height; i++) {
	  newImg->pixel[i] = 0;
  }

  return newImg;
}

/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
/// Ensures: (*imgp)==NULL.
/// Should never fail, and should preserve global errno/errCause.
void ImageDestroy(Image* imgp) { ///
  assert (imgp != NULL);

  if (*imgp != NULL) {
	  free((*imgp)->pixel);
	  free((*imgp));
	  *imgp = NULL;
  }
}


/// PGM file operations

// See also:
// PGM format specification: http://netpbm.sourceforge.net/doc/pgm.html

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PGM file.
/// Only 8 bit PGM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageLoad(const char* filename) { ///
  int w, h;
  int maxval;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  int success = 
  check( (f = fopen(filename, "rb")) != NULL, "Open failed" ) &&
  // Parse PGM header
  check( fscanf(f, "P%c ", &c) == 1 && c == '5' , "Invalid file format" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &w) == 1 && w >= 0 , "Invalid width" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d ", &h) == 1 && h >= 0 , "Invalid height" ) &&
  skipComments(f) >= 0 &&
  check( fscanf(f, "%d", &maxval) == 1 && 0 < maxval && maxval <= (int)PixMax , "Invalid maxval" ) &&
  check( fscanf(f, "%c", &c) == 1 && isspace(c) , "Whitespace expected" ) &&
  // Allocate image
  (img = ImageCreate(w, h, (uint8)maxval)) != NULL &&
  // Read pixels
  check( fread(img->pixel, sizeof(uint8), w*h, f) == w*h , "Reading pixels" );
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (!success) {
    errsave = errno;
    ImageDestroy(&img);
    errno = errsave;
  }
  if (f != NULL) fclose(f);
  return img;
}

/// Save image to PGM file.
/// On success, returns nonzero.
/// On failure, returns 0, errno/errCause are set appropriately, and
/// a partial and invalid file may be left in the system.
int ImageSave(Image img, const char* filename) { ///
  assert (img != NULL);
  int w = img->width;
  int h = img->height;
  uint8 maxval = img->maxval;
  FILE* f = NULL;

  int success =
  check( (f = fopen(filename, "wb")) != NULL, "Open failed" ) &&
  check( fprintf(f, "P5\n%d %d\n%u\n", w, h, maxval) > 0, "Writing header failed" ) &&
  check( fwrite(img->pixel, sizeof(uint8), w*h, f) == w*h, "Writing pixels failed" ); 
  PIXMEM += (unsigned long)(w*h);  // count pixel memory accesses

  // Cleanup
  if (f != NULL) fclose(f);
  return success;
}


/// Information queries

/// These functions do not modify the image and never fail.

/// Get image width
int ImageWidth(Image img) { ///
  assert (img != NULL);
  return img->width;
}

/// Get image height
int ImageHeight(Image img) { ///
  assert (img != NULL);
  return img->height;
}

/// Get image maximum gray level
int ImageMaxval(Image img) { ///
  assert (img != NULL);
  return img->maxval;
}

/// Pixel stats
/// Find the minimum and maximum gray levels in image.
/// On return,
/// *min is set to the minimum gray level in the image,
/// *max is set to the maximum.
void ImageStats(Image img, uint8* min, uint8* max) { ///
  assert (img != NULL);
  // Insert your code here!
  int x, y;

  for (x = 0; x < img->width; x++) {
	for (y = 0; y < img->height; y++) {
	  uint8 pixelColor = ImageGetPixel(img, x, y);
	  if (*min > pixelColor) {
		*min = pixelColor;
	  }
	  else if (*max < pixelColor) {
		*max = pixelColor;
	  }
	}
  }
}

/// Check if pixel position (x,y) is inside img.
int ImageValidPos(Image img, int x, int y) { ///
  assert (img != NULL);
  return (0 <= x && x < img->width) && (0 <= y && y < img->height);
}

/// Check if rectangular area (x,y,w,h) is completely inside img.
int ImageValidRect(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);
  // Insert your code here!
  // verifies if pixel position is inside img (invokes ImageValidPos method)
  assert(ImageValidPos(img, x, y)); 
  // w is img->width minus the value x, and h is img->width minus the value of y                                            
  return (0 <= w && w < img->width-x) && (0 <= h && h < img->height-y); 
}

/// Pixel get & set operations

/// These are the primitive operations to access and modify a single pixel
/// in the image.
/// These are very simple, but fundamental operations, which may be used to 
/// implement more complex operations.

// Transform (x, y) coords into linear pixel index.
// This internal function is used in ImageGetPixel / ImageSetPixel. 
// The returned index must satisfy (0 <= index < img->width*img->height)
static inline int G(Image img, int x, int y) {
  int index;
  
  // Insert your code here!
  index = x + (y * img->width);
  
  assert (index >= 0 && index < (img->width*img->height));    
  return index;
}

/// Get the pixel (level) at position (x,y).
uint8 ImageGetPixel(Image img, int x, int y) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (read)
  return img->pixel[G(img, x, y)];
} 

/// Set the pixel at position (x,y) to new level.
void ImageSetPixel(Image img, int x, int y, uint8 level) { ///
  assert (img != NULL);
  assert (ImageValidPos(img, x, y));
  PIXMEM += 1;  // count one pixel access (store)
  img->pixel[G(img, x, y)] = level;
} 


/// Pixel transformations

/// These functions modify the pixel levels in an image, but do not change
/// pixel positions or image geometry in any way.
/// All of these functions modify the image in-place: no allocation involved.
/// They never fail.


/// Transform image to negative image.
/// This transforms dark pixels to light pixels and vice-versa,
/// resulting in a "photographic negative" effect.
void ImageNegative(Image img) { ///
  assert (img != NULL);
  // Insert your code here!
  int x, y;

  for (x = 0; x < img->width; x++) {
	  for (y = 0; y < img->height; y++) {
      // Calculates new pixelColor, subtracting PixMax with the former pixelColor, inverting the colour.
	    uint8 pixelColor = PixMax - ImageGetPixel(img, x, y);     
      ImageSetPixel(img, x, y, pixelColor);                     
    }
  }
}

/// Apply threshold to image.
/// Transform all pixels with level<thr to black (0) and
/// all pixels with level>=thr to white (maxval).
void ImageThreshold(Image img, uint8 thr) { ///
  assert (img != NULL);
  // Insert your code here!

  int x, y;
  //Changes the colour of the pixel according to the value of the threshold given
  //It'll change to white whenever the value of the colour is >= to the threshold
  //And change to black if the previous isn't verified
  for (x = 0; x < img->width; x++) {
	  for (y = 0; y < img->height; y++) {
	    if (ImageGetPixel(img, x, y) < thr){           
        ImageSetPixel(img, x, y, 0);
      }
      else {
        ImageSetPixel(img, x, y, ImageMaxval(img));
      }

    }
  }
}

/// Brighten image by a factor.
/// Multiply each pixel level by a factor, but saturate at maxval.
/// This will brighten the image if factor>1.0 and
/// darken the image if factor<1.0.
void ImageBrighten(Image img, double factor) { ///
  assert (img != NULL);
  assert (factor >= 0.0);
  // Insert your code here!
  int x, y;

  for (x = 0; x < img->width; x++) {
	  for (y = 0; y < img->height; y++) {
	    uint8 pixelColor = ImageGetPixel(img, x, y);          
      pixelColor = (uint8) (pixelColor * factor + 0.5);   // This will round any value without using the math library
                                                        // if the result of the multiplication is, for example, 2.7, adding 0.5 will be 3.2, which will be 3 when typecasted to int
                                                        // if the result of the multiplication is, for example, 2.2, adding 0.5 will be 2.7, which will be 2 when typecasted to int
      ImageSetPixel(img, x, y, pixelColor);
    }
  }
}


/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
/// 
/// Success and failure are treated as in ImageCreate:
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.

// Implementation hint: 
// Call ImageCreate whenever you need a new image!

/// Rotate an image.
/// Returns a rotated version of the image.
/// The rotation is 90 degrees clockwise.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageRotate(Image img) { ///
  assert (img != NULL);
  int nWidth = img->height;
  int nHeight = img->width;
  uint8 maxValue = img->maxval;
  //Create a new image that'll be rotated
  Image nImg = ImageCreate(nWidth, nHeight, maxValue);

  if (nImg == NULL) {
	  return NULL;
  }
  for (int i = 0; i < nWidth; i++) {
	  for (int y = 0; y < nHeight; y++) {
	    //ImageSetPixel(nImg, nWidth - 1 - i, y, ImageGetPixel(img, y, i));       /// Rotação de 90º em sentido do relógio (não passa os testes)
      ImageSetPixel(nImg,  i, img->width-1-y, ImageGetPixel(img, y, i));        /// Rotação de 90º em sentido contra-relógio (passa nos testes)
	  }
  }

  return nImg;

}

/// Mirror an image = flip left-right.
/// Returns a mirrored version of the image.
/// Ensures: The original img is not modified.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageMirror(Image img) { ///
  assert (img != NULL);
  int nHeight = img->height;
  int nWidth = img->width;
  uint8 maxValue = img->maxval;
  // Create a new image that'll be a mirror of the original image
  Image nImg = ImageCreate(nWidth, nHeight, maxValue);

  if (nImg == NULL) {
	  return NULL;
  }
  // Goes through every position of the freshly created image
  for (int i = 0; i < nWidth; i++) {
	  for (int y = 0; y < nHeight; y++) {
      //The value for y remains the same
      //The value of x is changed, so that the first x becomes the last, and the last becomes the first
      //nWidth-1 is the last position that x can have. Then the current position is subtracted to the last position.
	    ImageSetPixel(nImg, nWidth - 1 - i, y, ImageGetPixel(img, i, y));
      //This way, in a 8 by 8 image, for example, y of the new image will always be the current position
      //While x will be follow the formula: x=7-currentPosition
	  }
  }

  return nImg;
}

/// Crop a rectangular subimage from img.
/// The rectangle is specified by the top left corner coords (x, y) and
/// width w and height h.
/// Requires:
///   The rectangle must be inside the original image.
/// Ensures:
///   The original img is not modified.
///   The returned image has width w and height h.
/// 
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
/// On failure, returns NULL and errno/errCause are set accordingly.
Image ImageCrop(Image img, int x, int y, int w, int h) { ///
  assert (img != NULL);
  assert (ImageValidRect(img, x, y, w, h));
  // Insert your code here!
  
  uint8 maxValue = img->maxval;
  Image nImg = ImageCreate(w, h, maxValue);                         // Creates a new white Image with w width and h height
  
  
  if (nImg == NULL) {
	  return NULL;
  }
  for (int i=0; i<w; i++){
    for(int j=0; j<h; j++){
      // Sets each pixel in the new image in the position (i,j) after obtaining the pixel of the original image in the position (x+i, y+j)
      ImageSetPixel(nImg, i, j, ImageGetPixel(img, x+i, y+j));    
    }
  }
  
  return nImg;
}


/// Operations on two images

/// Paste an image into a larger image.
/// Paste img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
void ImagePaste(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));
  // Insert your code here!
  //Goes through every position of image2
  for (int i=0; i<img2->width; i++){
    for (int j=0; j<img2->height; j++){                           
      ImageSetPixel(img1, i+x, j+y, ImageGetPixel(img2,  i, j)); //Set the pixel of the img2 in the position on img1, in the position it originally was in img2 + the values of x and y
    }
  }
}

/// Blend an image into a larger image.
/// Blend img2 into position (x, y) of img1.
/// This modifies img1 in-place: no allocation involved.
/// Requires: img2 must fit inside img1 at position (x, y).
/// alpha usually is in [0.0, 1.0], but values outside that interval
/// may provide interesting effects.  Over/underflows should saturate.
void ImageBlend(Image img1, int x, int y, Image img2, double alpha) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidRect(img1, x, y, img2->width, img2->height));
  // Insert your code here!
  //Goes through every position of image2
  for (int i=0; i<img2->width; i++){
    for (int j=0; j<img2->height; j++){  
      //Blends image1 and image2, according to a value alpha given, rounding the final value given
      //Follows the formula (1-alpha)*(Pixel(img1))+(alpha)*(Pixel(img2))
      uint8 blend = (uint8)( (1-alpha) * ImageGetPixel(img1,i+x,j+y) + (alpha) * ImageGetPixel(img2,i,j) +0.5);
      //Stores the value in the correct position in the image1, which will be the current position plus the position given by (x,y)                       
      ImageSetPixel(img1, i+x, j+y, blend);
    }
  }
}

/// Compare an image to a subimage of a larger image.
/// Returns 1 (true) if img2 matches subimage of img1 at pos (x, y).
/// Returns 0, otherwise.
int ImageMatchSubImage(Image img1, int x, int y, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  assert (ImageValidPos(img1, x, y));
  // Insert your code here!
  int mWidth = img2->width;
  int mHeight = img2->height;
  //Goes through every position of image2
  for (int i = 0; i < mWidth; i++) {
	  for (int j = 0; j < mHeight; j++) {
      CountLocate+=1;
      //Compares if the pixels of image2 in the current position with the pixels of image1 in the position given by the sums x+current position && y+current position don't match
	    if (ImageGetPixel(img1, x + i, y + j) != ImageGetPixel(img2, i, j)) {
      //Returns 0 (false) if they are different
		    return 0;
	    }
	  }
  }
  //Returns 1 (true) if the image matches the subimage
  return 1;
}

/// Locate a subimage inside another image.
/// Searches for img2 inside img1.
/// If a match is found, returns 1 and matching position is set in vars (*px, *py).
/// If no match is found, returns 0 and (*px, *py) are left untouched.
int ImageLocateSubImage(Image img1, int* px, int* py, Image img2) { ///
  assert (img1 != NULL);
  assert (img2 != NULL);
  //Goes through every position (pixel) of the image1 as long as the image2 can still fit, i.e., up to img1->width-img2->width && img1->height-img2->height
  //There's no need to check the pixels further in the image past that point if the image2 as still not been found, because it wouldn't fit any longer 
  for (int i = 0; i<=img1->width-img2->width; i++){
    for (int j =0; j<=img1->height-img2->height; j++){
      //Checks if the pixel of image2 matches the pixel in image1 in the specified position
      if(ImageMatchSubImage(img1, i, j, img2)){
        //Alters the value of *px and *py, returning 1 (or true)
        
        *px = i;
        *py = j;
        return 1;
      }
    }
  }
  //Returns 0 (or false) if the image2 is not found within the confines of the image1
  return 0;
}


/// Filtering

/// Blur an image by a applying a (2dx+1)x(2dy+1) mean filter.
/// Each pixel is substituted by the mean of the pixels in the rectangle
/// [x-dx, x+dx]x[y-dy, y+dy].
/// The image is changed in-place.
void ImageBlur(Image img, int dx, int dy) { ///
  // Insert your code here!
  /*
  Image imgAuxiliar = ImageCreate(img->width, img->height, img->maxval);
  memcpy((void*)imgAuxiliar->pixel, (void*)img->pixel, img->width * img-> height);    //copies original image to the previously created Auxiliar Image.
  
  //Goes through every position of the original image
 
  for (int i = 0; i < img->width; i++) {
	  for (int j = 0; j < img->height; j++) {
	    int sum = 0;
	    int count = 0;

      // Goes through every position of the sub-image with dx width and dy height

	    for (int k = i - dx; k <= i + dx; k++) {
		    for (int l = j - dy; l <= j + dy; l++) {
          CountBlur+=1;
          //Checks if the position is valid
		      if (ImageValidPos(img, k, l)) {
            SumBlur+=1;
            //Sum the pixel of the auxiliar image and increments count.
			      sum += ImageGetPixel(imgAuxiliar, k, l);
			      count++;
		      }
		    }
	    }

    // Sets the value of the pixel in the original pixel. The value of the pixel is the average of the pixels in the vicinity, adjusted for rounding 
	  ImageSetPixel(img, i, j, (uint8) (((sum+count/2)/count)));
	  }
  }
  //destroys the Auxiliar image
  ImageDestroy(&imgAuxiliar);
  */
   //A more efficient method to calculate ImageBlur
  
  int h = img->height, w = img->width;
  uint32_t sum[h][w];

  // Sum of every pixel before the position aborded
  for (int i = 0; i < w; i++){
    for (int j = 0; j < h; j++){
      if(i == 0 && j == 0 ){
        sum[j][i] = (uint32_t)ImageGetPixel(img,i,j); // Value of the (x,y)
      }else if(i == 0){
        sum[j][i] = (uint32_t)(ImageGetPixel(img,i,j) + sum[j-1][i]);
        SumBlur+=1;
      }else if(j == 0){        
        sum[j][i] = (uint32_t)(ImageGetPixel(img,i,j) + sum[j][i-1]);
        SumBlur+=1;
      }else{
        sum[j][i] = (uint32_t)(ImageGetPixel(img,i,j) + sum[j][i-1] + sum[j-1][i] - sum[j-1][i-1]);
        SumBlur+=2;
      }
    }
  }

  for (int x = 0; x < w; x++){
    for (int y = 0; y < h; y++){
      CountBlur+=1;
      
      int xM = (x+dx<w-1) ? x+dx : w-1;
      int xm = (x-dx>0) ? x-dx : 0;
      int yM = (y+dy<h-1) ? y+dy : h-1;
      int ym = (y-dy>0) ? y-dy : 0;
      int count = (xM-xm +1)*(yM-ym+1); 


      // Searches in array the value of the sum
      uint32_t a = (ym < 1 || xm < 1 ) ? 0 : sum[ym -1][xm -1];
      uint32_t b = ym < 1 ? 0 : sum[ym - 1][xM];
      uint32_t c = xm < 1 ? 0 : sum[yM][xm-1];
      uint32_t d = sum[yM][xM];
      // Subtracts the areas of outside the area we are calculating, adding afterwards the area that was twice removed
      double soma = (double) d-b-c+a;
      // Sets the value of the pixel we want, rounded
      ImageSetPixel(img , x, y, (uint8)((soma+count/2)/count));
    }
  }
  
}



