#ifndef H_CWDAEMON_LP
#define H_CWDAEMON_LP




struct cwdev_s;




int lp_init(struct cwdev_s * dev, int fd);
int lp_free(struct cwdev_s * dev);
/// Reset pins of cwdevice to known states
int lp_reset_pins(struct cwdev_s * dev);
int lp_cw(struct cwdev_s * dev, int onoff);
int lp_ptt(struct cwdev_s * dev, int onoff);
int lp_ssbway(struct cwdev_s * dev, int onoff);
int lp_switchband(struct cwdev_s * dev, unsigned char bitpattern);
int lp_footswitch(struct cwdev_s * dev);




#endif /* #ifndef H_CWDAEMON_LP */

