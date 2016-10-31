#include "frame-process.h"
#include <stdlib.h>
#include <math.h>

//
uint8_t* resize_frame(uint8_t* data, int width, int height, int new_width, int new_height, int channel) {
    uint8_t* newData = malloc(sizeof(uint8_t) * channel * new_width*new_height);
    float wDiv = (float)width / new_width;
    float hDiv = (float)height / new_height;
    
    int pos, newPos;
    for (int j = 0; j < new_height; ++j) {
        for (int i = 0; i < new_width; ++i) {
            pos = channel * (round(j*hDiv)*width + round(i*wDiv));
            newPos = channel *(j*new_width + i);
            memcpy(&newData[newPos], &data[pos], channel);
        }
    }
    
    return newData;
}

//edge compare
int edge_compare(const void * a, const void * b) {
    const struct edge* e1 = (const struct edge*)a;
    const struct edge* e2 = (const struct edge*)b;
    
    return e1->weight - e2->weight;
    
}

//qsort compare
int compare (const void * a, const void * b)
{
    return ( *(int*)a - *(int*)b );
}

//average filter
void average_smooth(uint8_t* data, int width, int height) {
    int smooth_r =1;
    int boxsum,kh,kw;
    
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            boxsum = 0;
            
            for (int m=-smooth_r; m<smooth_r+1; ++m) {
                for (int n=-smooth_r; n<smooth_r+1; ++n) {
                    kh = h+m;
                    kw = w+n;
                    int_fit_in_range(&kw, 0, width-1);
                    int_fit_in_range(&kh, 0, height-1);
                    boxsum += data[kh*width+kw];
                }
            }
            boxsum /= (2*smooth_r+1)*(2*smooth_r+1);
            data[h*width+w] = boxsum;
        }
    }
}

//median filter
void median_smooth(uint8_t* data, int width, int height) {
    int smooth_r = 1;
    int kw,kh, count;
    uint8_t *arr = malloc(sizeof(uint8_t)*(2*smooth_r+1)*(2*smooth_r+1));
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            count = 0;
            for (int m=-smooth_r; m<smooth_r+1; ++m) {
                for (int n=-smooth_r; n<smooth_r+1; ++n) {
                    kh = h+m;
                    kw = w+n;
                    int_fit_in_range(&kw, 0, width-1);
                    int_fit_in_range(&kh, 0, height-1);
                    arr[count] = data[kh*width+kw];
                    ++count;
                }
            }
            qsort(arr, (2*smooth_r+1)*(2*smooth_r+1), sizeof(uint8_t), compare);
            data[h*width+w] = arr[(int)(2*smooth_r+1)*(2*smooth_r+1)/2];
        }
    }
}

//caluclate integral frame
int* calculate_integral_frame(uint8_t* data, int width, int height, int channel) {
    int* integral_frame = malloc(sizeof(int) * channel * width*height);
    
    int area_a, area_b, area_c;
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            
            for (int c=0; c<channel; ++c) {
                if (h>0 && w>0) {
                    area_a = integral_frame[ channel*( (h-1)*width+w-1 ) + c ];
                    area_b = integral_frame[ channel*( (h-1)*width+w) + c ];
                    area_c = integral_frame[ channel*( h*width+w-1) + c ];
                } else if (h>0) {
                    area_a = area_c = 0;
                    area_b = integral_frame[ channel*( (h-1)*width+w) + c ];
                } else if (w>0) {
                    area_a = area_b = 0;
                    area_c = integral_frame[ channel*( h*width+w-1) + c ];
                } else {
                    area_a = area_b = area_c = 0;
                }
                
                integral_frame[ channel*(h*width+w)+c ] = data[ channel*(h*width+w)+c ] + area_b + area_c - area_a;
            }
            
        }
    }
    return integral_frame;
}

//calculate grid frame
int* calculate_grid_frame(uint8_t* data, int width, int height, int grid_width, int grid_height, int channel) {
    int grid_w_num = ceil((float)width/grid_width);
    int grid_h_num = ceil((float)height/grid_height);
    
    int* integral_frame = calculate_integral_frame(data, width, height, channel);
    int* grid_frame = malloc(sizeof(int) * channel * grid_w_num*grid_h_num);
    
    int area_a, area_b, area_c, area_d;
    int h1, h2, w1, w2;
    for (int h = 0; h < grid_h_num; ++h) {
        for (int w = 0; w < grid_w_num; ++w) {
            h1 = h*grid_height;
            h2 = h*grid_height+grid_height-1;
            w1 = w*grid_width;
            w2 = w*grid_width+grid_width-1;
            
            for (int c=0; c<channel; ++c) {
                if (h2>=height)
                    h2 = height-1;
                if (w2>=width)
                    w2 = width-1;
                
                area_a = integral_frame[ channel*(h1*width+w1) + c ];
                area_b = integral_frame[ channel*(h1*width+w2) + c ];
                area_c = integral_frame[ channel*(h2*width+w1) + c ];
                area_d = integral_frame[ channel*(h2*width+w2) + c ];
                
                grid_frame[ channel*(h*grid_w_num+w)+c ] =  area_d - area_b - area_c + area_a;
            }
        }
    }
    free(integral_frame);
    return grid_frame;
}

void erode(uint8_t* data, int width, int height, int channel) {
    int threshold = 1;
    
    int* integral_data = calculate_integral_frame(data, width, height, channel);
    int pos, area_a, area_b, area_c, area_d;
    for (int h = 1; h < height-1; ++h) {
        for (int w = 1; w < width-1; ++w) {

            for (int c=0; c<channel; ++c) {
                if (data[channel*(h*width+w)+c] == 0)
                    continue;
                
                area_a = integral_data[channel*( (h-1)*width+w-1 ) +c];
                area_b = integral_data[channel*( (h-1)*width+w+1 ) +c];
                area_c = integral_data[channel*( (h+1)*width+w-1 ) +c];
                area_d = integral_data[channel*( (h+1)*width+w+1 ) +c];
                
                if (area_d+area_a-area_b-area_c < threshold)
                    data[channel*(h*width+w)+c] = 0;
            }
            
        }
    }
    free(integral_data);
}

void dilate(uint8_t* data, int width, int height, int channel) {
    uint8_t* old_data = malloc(sizeof(uint8_t)*width*height*channel);
    memcpy(old_data, data, sizeof(uint8_t)*width*height*channel);
    
    for (int h = 1; h < height-1; ++h) {
        for (int w = 1; w < width-1; ++w) {
            
            for (int c=0; c<channel; ++c) {
                if (old_data[channel*(h*width+w)+c] == 0)
                    continue;
                
                for (int m=-1; m<=1; ++m) {
                    for (int n=-1; n<=1; ++n) {
                        data[channel*((h+m)*width+w+n)+c] = data[channel*(h*width+w)+c];
                    }
                }
            }
            
        }
    }
    free(old_data);
}


struct bounding_box* find_bounding_box(uint8_t* data, int width, int height, struct bounding_box *pre_bounding_box) {
    //struct bounding_box default_bounding_box = { {0, 0}, {width, height}, { width/2, height/2 } };
    int grid_width = 20;
    int grid_height = 20;
    
    int* grid_frame = calculate_grid_frame(data, width, height, grid_width, grid_height, 1);
    int grid_w_num = ceil((float)width/grid_width);
    int grid_h_num = ceil((float)height/grid_height);
    
    int threshold = 5;
    
    //init edges
    struct edge *edges = malloc(sizeof(struct edge)*grid_w_num*grid_h_num*4);
    struct point *grids = malloc(sizeof(struct point)*grid_w_num*grid_h_num);
    points **labels = malloc(sizeof(struct points*)*grid_w_num*grid_h_num);
    
    for(int h=0; h<grid_h_num; ++h) {
        for (int w=0; w<grid_w_num; ++w) {
            grids[h*grid_w_num+w].x = w;
            grids[h*grid_w_num+w].y = h;
            labels[h*grid_w_num+w] = NULL;
        }
    }
    
    int edge_count = 0;
    for(int h=0; h<grid_h_num; ++h) {
        for (int w=0; w<grid_w_num; ++w) {
            
            if( grid_frame[h*grid_w_num + w] < threshold)
                continue;
            
            if (w<grid_w_num-1 && grid_frame[h*grid_w_num + w+1] > threshold) {
                edges[edge_count].p1 = &grids[h*grid_w_num + w];
                edges[edge_count].p2 = &grids[h*grid_w_num + w+1];
                edges[edge_count].weight = 1;
                edge_count++;
            }
            
            if (h<grid_h_num-1 && grid_frame[(h+1)*grid_w_num + w] > threshold) {
                edges[edge_count].p1 = &grids[h*grid_w_num + w];
                edges[edge_count].p2 = &grids[(h+1)*grid_w_num + w];
                edges[edge_count].weight = 1;
                edge_count++;
            }
            
            if (h>0 && w<grid_w_num-1 &&  grid_frame[(h-1)*grid_w_num + w+1] > threshold) {
                edges[edge_count].p1 = &grids[h*grid_w_num + w];
                edges[edge_count].p2 = &grids[(h-1)*grid_w_num + w+1];
                edges[edge_count].weight = 1;
                edge_count++;
            }
            
            if (h<grid_h_num-1 && w<grid_w_num-1 && grid_frame[(h+1)*grid_w_num + w+1] > threshold) {
                edges[edge_count].p1 = &grids[h*grid_w_num + w];
                edges[edge_count].p2 = &grids[(h+1)*grid_w_num + w+1];
                edges[edge_count].weight = 1;
                edge_count++;
            }
        }
    }
    
    //segment grids
    DARRAY(points*) segments;
    da_init(segments);
    DARRAY(struct point*) *seg, *seg1, *seg2;
    
    struct point *p, *p1, *p2;
    int segment_count = 0;
    for (int i=0; i<edge_count; ++i) {
        //blog(LOG_INFO, "%d %d %d|", edges[i].p1->x, i, edge_count);
        p1 = edges[i].p1;
        p2 = edges[i].p2;
        seg1 = labels[p1->y*grid_w_num+p1->x];
        seg2 = labels[p2->y*grid_w_num+p2->x];
        
        if (seg1 == NULL && seg2 == NULL) {
            seg = malloc(sizeof(points));
            da_init((*seg));
            da_push_back( (*seg), &p1);
            da_push_back( (*seg), &p2);
            da_push_back(segments, &seg);
            segment_count++;
            labels[p2->y*grid_w_num+p2->x] = seg;
        } else if (seg1 == NULL) {
            da_push_back( (*seg2), &p1);
            labels[p1->y*grid_w_num+p1->x] = seg2;
        } else if (seg2 == NULL) {
            da_push_back( (*seg1), &p2);
            labels[p2->y*grid_w_num+p2->x] = seg1;
        } else if (seg1 != seg2){
            for (int j=0; j<seg2->num; ++j) {
                p = seg2->array[j];
                labels[p->y*grid_w_num+p->x] = seg1;
            }
            da_join((*seg1), (*seg2));
        }
    }
    
    struct bounding_box *res_bounding_box = NULL;
    if (segments.num <= 0) {
        blog(LOG_INFO, "no segments");
        goto freeall;
    }
    
    //find maximum segment
    seg = segments.array[0];
    int max_size = seg->num;
    for (int i=1; i<segments.num; ++i) {
        if (segments.array[i]->num > max_size) {
            max_size = segments.array[i]->num;
            seg = segments.array[i];
        }
    }

    //get bounding box
    int left, right, top, bot;
    left = width-1;
    right = 0;
    top = height-1;
    bot = 0;
    for (int i=0; i<seg->num; ++i) {
        p = seg->array[i];
        if (p->x < left)
            left = p->x;
        if (p->x > right)
            right = p->x;
        if (p->y < top)
            top = p->y;
        if (p->y > bot)
            bot = p->y;
    }
    left *= grid_width;
    right *= grid_width;
    top *= grid_height;
    bot *= grid_height;
    
    //blog(LOG_INFO, "seg->num: %d", seg->num);
    
    struct bounding_box tmp_bd_box = { {left, top}, {right, bot}, { (left+right)/2, (top+bot)/2 } };
    res_bounding_box = malloc(sizeof(struct bounding_box));
    memcpy(res_bounding_box, &tmp_bd_box, sizeof(struct bounding_box));
    
    for (int i=0; i<segments.num; ++i) {
        da_free((*segments.array[i]));
    }

freeall:
    da_free(segments);

    free(grid_frame);
    free(edges);
    free(grids);
    free(labels);
    
    return res_bounding_box;
}

uint8_t* find_motion_mask(uint8_t* old_framedata, uint8_t *framedata, int width, int height, int channel) {
    //resize frame to avoid too much calculation
    int resize_width = 600;
    int resize_height = 400;
    uint8_t* resized_old_frame = resize_frame(old_framedata, width, height, resize_width, resize_height, channel);
    uint8_t* resized_frame = resize_frame(framedata, width, height, resize_width, resize_height, channel);
    
    uint8_t* mask = malloc(sizeof(uint8_t)*resize_width*resize_height);
    uint8_t* diff = malloc(sizeof(uint8_t)*channel);
    
    int pos;
    int threshold = 30;
    for (int h = 0; h < resize_height; ++h) {
        for (int w = 0; w < resize_width; ++w) {
            pos = h*resize_width+w;
            for (int c=0; c<channel; ++c) {
                diff[c] = abs((int)resized_frame[channel*pos+c]-resized_old_frame[channel*pos+c]);
            }
            if (diff[0]<threshold && diff[1]<threshold && diff[2]<threshold){
                mask[pos] = 0;
            } else {
                mask[pos] = 1;
                //mask[pos] = (diff[0]+diff[1]+diff[2])/3;
            }
        }
    }
    
    //median_smooth(mask, width, height);
    erode(mask, resize_width, resize_height, 1);
    dilate(mask, resize_width, resize_height, 1);
    uint8_t* res_mask = resize_frame(mask, resize_width, resize_height, width, height, 1);
    
    free(mask);
    free(diff);
    free(resized_old_frame);
    free(resized_frame);
    return res_mask;
}


