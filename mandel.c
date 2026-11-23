/***************************************
 * @file mandel.c
 * @author Joseph Hernandez
 * @date 11/22/26
 * @assignment CPE2600-111 Lab 11
 *
 * Without -p, the program generates one JPEG image.
 *
 * With -p, the program generates a video with 50 frames of JPEG images using
 * the specified number of processes.
 *
 * compile with:
 * Makefile
 *
 * ./mandel -p 5 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg
 * ffmpeg -y -framerate 10 -start_number 0 -i mandel%d.jpg -c:v mpeg1video -q:v 2 -pix_fmt yuv420p -strict unofficial -loglevel error mandel.mpg 
 * ffplay mandel.mpg
 *
 * ffprobe -v error -show_entries format=duration -show_entries stream=width,height,r_frame_rate -of default=noprint_wrappers=1 mandel.mpg - gives config info about the movie
 *
 * compare time of # of processors: 
 * time ./mandel -p 1 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 4 min 18.172 s
 * time ./mandel -p 2 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 2 min 6.384 s
 * time ./mandel -p 5 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 0 min 49.862 s
 * time ./mandel -p 10 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 0 min 32.544 s
 * time ./mandel -p 20 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 0 min 31.520 s
 * BONUS : 
 * time ./mandel -p 13 -x -0.06699218749999991 -y -0.65625 -s 2.0 -e 0.0703125 -m 1000 -W 1000 -H 1000 -o mandel.jpg -- 0 min 28.932 s
 ***************************************/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "jpegrw.h"

#define FRAME_COUNT 50

static int iteration_to_color(int iterations, int max);
static int iterations_at_point(double x, double y, int max);
static void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max);
static int generate_image(double xcenter, double ycenter, double xscale, int width, int height, int max, const char *outfile);
static int run_movie(int process_limit, double xcenter, double ycenter, double start_scale, double end_scale, int width, int height, int max, const char *outfile);
static char *make_frame_filename(const char *base, int frame);
static int parse_integer(const char *text, int *value);
static int parse_double_value(const char *text, double *value);
static void show_help();


/*
 * The main program has two modes
 *
 * 1. Single image:
 *    ./mandel -x -0.5 -y 0 -s 0.5
 *
 * 2. 50 frame movie:
 *    ./mandel -p 5 -x -0.5 -y 0 -s 0.5
 *
 * The -p (number of processes) command line arg gives movies, image is default
 */
int main(int argc, char *argv[]) {
    int option;

    // default image
    const char *outfile = "mandel.jpg";
    double xcenter = 0.0;
    double ycenter = 0.0;
    double xscale = 4.0;
    double end_scale = 0.0;
    int image_width = 1000;
    int image_height = 1000;
    int max_iterations = 1000;
    
    int process_limit = 1;
    int movie_mode = 0;
    int end_scale_set = 0;

    /*
     * check all command line options using getopt and any of the following options
     *
     * -x = X coordinate of center of image
     * -y = Y coordinate of center of image
     * -s = horizontal image scale
     * -W = image width
     * -H = image height
     * -m = maximum iterations
     * -o = output filename
     * -h = help
     * 
     * -p = number of simultaneous child processes
     * -e = ending image scale for movie mode
     */
    while ((option = getopt(argc, argv, "x:y:s:e:W:H:m:o:p:h")) != -1) {
        switch (option) {
            case 'x':
                if (!parse_double_value(optarg, &xcenter)) {
                    fprintf(stderr, "Invalid X coordinate.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'y':
                if (!parse_double_value(optarg, &ycenter)) {
                    fprintf(stderr, "Invalid Y coordinate.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 's':
                if (!parse_double_value(optarg, &xscale) || xscale <= 0.0) {
                    fprintf(stderr, "Scale must be positive.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'e':
                if (!parse_double_value(optarg, &end_scale)) {
                    fprintf(stderr, "Invalid ending scale.\n");
                    return EXIT_FAILURE;
                }
                end_scale_set = 1;
                break;

            case 'W':
                if (!parse_integer(optarg, &image_width) || image_width <= 0) {
                    fprintf(stderr, "Invalid image width.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'H':
                if (!parse_integer(optarg, &image_height) || image_height <= 0) {
                    fprintf(stderr, "Invalid image height.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'm':
                if (!parse_integer(optarg, &max_iterations) || max_iterations <= 0) {
                    fprintf(stderr, "Invalid maximum iterations.\n");
                    return EXIT_FAILURE;
                }
                break;

            case 'o':
                outfile = optarg;
                break;

            case 'p':
                if (!parse_integer(optarg, &process_limit)) {
                    fprintf(stderr, "Invalid process count.\n");
                    return EXIT_FAILURE;
                }
                movie_mode = 1;
                break;

            case 'h':
                show_help();
                return EXIT_SUCCESS;

            default:
                show_help();
                return EXIT_FAILURE;
        }
    }

    // for unexpected arg that arent found above
    if (optind != argc) {
        fprintf(stderr, "Unexpected command-line argument.\n");
        show_help();
        return EXIT_FAILURE;
    }

    // make sure the image settings are not negative
    if (image_width <= 0 || image_height <= 0 || max_iterations <= 0 || xscale <= 0.0) {
        fprintf(stderr, "Invalid image settings.\n");
        return EXIT_FAILURE;
    }

    // if -p is invoked
    if (movie_mode) {
        // Only allow as many processes as there are frames being made
        if (process_limit < 1 || process_limit > FRAME_COUNT) {
            fprintf(stderr, "Process count must be between 1 and %d.\n", FRAME_COUNT);
            return EXIT_FAILURE;
        }

        // if -e arg not specified ending scale '-e' = xscale / 1000
        if (!end_scale_set) {
            end_scale = xscale / 1000.0;
        }

        // check ending scale to see if it is pos and smaller
        if (end_scale <= 0.0 || end_scale >= xscale) {
            fprintf(stderr, "Ending scale must be positive and smaller than starting scale.\n");
            return EXIT_FAILURE;
        }

        printf("Mandelbrot movie settings:\n");
        printf("Center X:        %.15f\n", xcenter);
        printf("Center Y:        %.15f\n", ycenter);
        printf("Starting scale:  %g\n", xscale);
        printf("Ending scale:    %g\n", end_scale);
        printf("Image width:     %d\n", image_width);
        printf("Image height:    %d\n", image_height);
        printf("Max iterations:  %d\n", max_iterations);
        printf("Total frames:    %d\n", FRAME_COUNT);
        printf("Child processes: %d\n\n", process_limit);

        fflush(stdout);
        return run_movie(process_limit, xcenter, ycenter, xscale, end_scale, image_width, image_height, max_iterations, outfile);
    }

    // if -p was not supplied
    // make sure an -e value wasnt unnecessarily set for the still image
    if (end_scale_set) {
        fprintf(stderr, "The -e option requires movie mode (-p).\n");
        return EXIT_FAILURE;
    }

    printf("Mandelbrot image settings:\n");
    printf("Center X:       %lf\n", xcenter);
    printf("Center Y:       %lf\n", ycenter);
    printf("Scale:          %lf\n", xscale);
    printf("Image width:    %d\n", image_width);
    printf("Image height:   %d\n", image_height);
    printf("Max iterations: %d\n", max_iterations);
    printf("Output file:    %s\n\n", outfile);

    if (generate_image(xcenter, ycenter, xscale, image_width, image_height, max_iterations, outfile) != 0) {
        return EXIT_FAILURE;
    }

    printf("Image saved successfully: %s\n", outfile);
    return EXIT_SUCCESS;
}


/*
 * Generates one Mandelbrot image
 *
 * this is used by both modes of the mandel.c file
 * - single image
 * - movie
 *
 * returns:
 * 0 = success
 * 1 = fail
 */

static int generate_image(double xcenter, double ycenter, double xscale, int width, int height, int max, const char *outfile) {
    imgRawImage *img;
    double yscale;

    //calculate vertical scale
    yscale = xscale * ((double)height / width);

    img = initRawImage((unsigned int)width, (unsigned int)height);

    if (img == NULL) {
        fprintf(stderr, "Unable to allocate image memory.\n");
        return 1;
    }

    // set image to black initially
    setImageCOLOR(img, 0x000000);

    compute_image(img, xcenter - xscale / 2.0, xcenter + xscale / 2.0, ycenter - yscale / 2.0, ycenter + yscale / 2.0, max);

    // save image as a jpeg
    if (storeJpegImageFile(img, outfile) != 0) {
        fprintf(stderr, "Unable to save image: %s\n", outfile);
        freeRawImage(img);
        return 1;
    }
    freeRawImage(img);

    return 0;
}


// create the 50 frames using a specified amount of processes
static int run_movie(int process_limit, double xcenter, double ycenter, double start_scale, double end_scale, int width, int height, int max, const char *outfile) {
    int next_frame = 0;
    int active_children = 0;
    int completed_frames = 0;
    int stop_launching = 0;
    int had_error = 0;

    // go until all frames are computed or there are no children working
    while (next_frame < FRAME_COUNT || active_children > 0) {
        while (next_frame < FRAME_COUNT && active_children < process_limit && !stop_launching) {
            pid_t pid;
            double fraction;
            double current_scale;
            char *frame_filename;

            /*
             * calculate the zoom 
             *
             * frame 0 uses the starting scale.
             * frame 49 uses the ending scale.
             */

            fraction = (double)next_frame / (FRAME_COUNT - 1);
            current_scale = start_scale * pow(end_scale / start_scale, fraction);
            // assign unique name to each frame
            frame_filename = make_frame_filename(outfile, next_frame);
            // make sure none of the names fail to be made
            if (frame_filename == NULL) {
                fprintf(stderr, "Unable to create frame filename.\n");
                stop_launching = 1;
                had_error = 1;
                break;
            }
            fflush(stdout);

            // create child process
            pid = fork();
            // make sure fork worked
            if (pid < 0) {
                perror("fork");
                free(frame_filename);
                stop_launching = 1;
                had_error = 1;
                break;
            }

            if (pid == 0)
            {
                // child process
                int result;
                result = generate_image(xcenter, ycenter, current_scale, width, height, max, frame_filename);
                free(frame_filename);

                _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
            }

            // parent process
            printf("Started frame %d: %s (PID %ld)\n", next_frame, frame_filename, (long)pid);
            free(frame_filename);
            // keep counts of child process and frames
            active_children++;
            next_frame++;
        }

        // wait for a child to finish to fill that spot
        if (active_children > 0) {
            pid_t finished_pid;
            int status;

            do {
                finished_pid = wait(&status);

            } while (finished_pid == -1 && errno == EINTR);

            if (finished_pid == -1) {
                perror("wait");
                had_error = 1;
                break;
            }
            active_children--;

            // make sure child exited successfully
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                completed_frames++;
                printf("Child %ld completed successfully.\n", (long)finished_pid);
            } else {
                fprintf(stderr, "Child %ld failed to generate its frame.\n", (long)finished_pid);
                had_error = 1;
            }
        }

        // if failure, wait for children 
        if (stop_launching && active_children == 0) {
            break;
        }
    }

    //print results
    printf("\nMovie generation results:\n");
    printf("Frames started:   %d\n", next_frame);
    printf("Frames completed: %d\n", completed_frames);

    if (had_error || next_frame != FRAME_COUNT || completed_frames != FRAME_COUNT) {
        fprintf(stderr, "Movie generation was not fully successful.\n");
        return EXIT_FAILURE;
    }

    printf("\nAll %d frames generated successfully!\n", FRAME_COUNT);
    return EXIT_SUCCESS;
}


// make file name for each frame i.e: mandel.c -> mandel0.c, mandel1.c, ... mandel49.c
static char *make_frame_filename(const char *base, int frame) {
    size_t prefix_length;
    char *filename;

    prefix_length = strlen(base);

    // check if .jpg was written
    if (prefix_length >= 4 && strcmp(base + prefix_length - 4, ".jpg") == 0) {
        prefix_length -= 4;
    } else if (prefix_length >= 5 && strcmp(base + prefix_length - 5, ".jpeg") == 0) {
        prefix_length -= 5;
    }

    // allocate memory for the names of each fram
    filename = malloc(prefix_length + 32);
    if (filename == NULL) {
        return NULL;
    }
    // take original name and add frame number
    memcpy(filename, base, prefix_length);
    snprintf(filename + prefix_length, 32, "%d.jpg", frame);

    return filename;
}


/*
 * Calculate the number of iterations for one point.
 * z(n+1) = z(n)^2 + c
 * where z is initially 0
 */
static int iterations_at_point(double x, double y, int max) {
    double x0 = x;
    double y0 = y;
    double zx = 0.0;
    double zy = 0.0;
    double xt;
    int iter = 0;

    while ((zx * zx + zy * zy <= 4.0) && iter < max) {
        xt = zx * zx - zy * zy + x0;
        zy = 2.0 * zx * zy + y0;
        zx = xt;
        iter++;
    }
    return iter;
}


// given the coordinates find the mandel image from the position
static void compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double ymax, int max) {
    unsigned int x;
    unsigned int y;
    unsigned int width = img->width;
    unsigned int height = img->height;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            double real;
            double imaginary;
            int iterations;

            // seperate position to complex number
            real = xmin + (double)x * (xmax - xmin) / width;
            imaginary = ymin + (double)y * (ymax - ymin) / height;
            // find iterations at that point
            iterations = iterations_at_point(real, imaginary, max);
            // set color based on iterations
            setPixelCOLOR(img, x, y, iteration_to_color(iterations, max));
        }
    }
}

static int iteration_to_color(int iterations, int max) {
    double t;
    int r;
    int g;
    int b;
    if (iterations >= max) {
        return 0x000000;
    }
    t = sqrt((double)iterations / (double)max);

    // smooth polynomial RGB gradient
    r = (int)(9.0 * (1.0 - t) * t * t * t * 255.0);
    g = (int)(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0);
    b = (int)(8.5 * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0);

    return (r << 16) | (g << 8) | b;
}

// command line string to an int
static int parse_integer(const char *text, int *value) {
    char *endptr;
    long result;
    errno = 0;
    result = strtol(text, &endptr, 10);

    if (errno != 0 || endptr == text || *endptr != '\0' || result < INT_MIN || result > INT_MAX) {
        return 0;
    }
    *value = (int)result;
    return 1;
}


// command line string to double
static int parse_double_value(const char *text, double *value) {
    char *endptr;
    double result;
    errno = 0;
    result = strtod(text, &endptr);

    if (errno != 0 || endptr == text || *endptr != '\0' || !isfinite(result)) {
        return 0;
    }
    *value = result;
    return 1;
}


// display usage
static void show_help() {
    printf("Usage: Mandel options\n\n");
    printf("Original image options:\n");
    printf("-x <coord>    X coordinate of image center.\n");
    printf("-y <coord>    Y coordinate of image center.\n");
    printf("-s <scale>    Horizontal image scale.\n");
    printf("-W <pixels>   Image width.\n");
    printf("-H <pixels>   Image height.\n");
    printf("-m <max>      Maximum iterations.\n");
    printf("-o <file>     Output filename or movie prefix.\n");
    printf("-p <number>   Number of simultaneous child processes.\n");
    printf("-e <scale>    Ending scale for movie mode.\n");
    printf("-h            Display help message.\n");
    printf("\nExamples:\n");
    printf("Single image:\n");
    printf("./mandel -x -0.5 -y 0 -s 0.5\n");
    printf("\nMovie using five processes:\n");
    printf("./mandel -p 5 -x -0.5 -y 0 -s 0.5 -e 0.001\n");
}