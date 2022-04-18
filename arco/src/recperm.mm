#include <AVFoundation/AVFoundation.h>

void request_record_permission() {
    // first, see if we need to request permission
    NSOperatingSystemVersion version = {10, 14, 0};
    if ([[NSProcessInfo processInfo]
         isOperatingSystemAtLeastVersion: version]) {
        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio
                                 completionHandler: ^(BOOL granted) {
            if (granted) {
                NSLog(@"req granted");
            }
        }];
    }
}

int request_record_status() {
    AVAuthorizationStatus authStatus =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    return (int) authStatus;
}
