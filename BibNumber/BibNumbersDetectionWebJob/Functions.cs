using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.WebJobs;
using Newtonsoft.Json;
using BibNumberWeb;
using System.Net;
using BibNumberWrapper;

namespace BibNumbersDetectionWebJob
{
    public class Functions
    {
        // This function will get triggered/executed when a new message is written 
        // on an Azure Queue called queue.
        public static void ProcessQueueMessage([QueueTrigger("detectbibnumbersqueue")] string message)
        {
            var photo = JsonConvert.DeserializeObject<Photo>(message);

            var tempFile = Path.GetTempFileName();
            using (WebClient webClient = new WebClient())
            {
                webClient.DownloadFile(photo.Url, tempFile);
                Class1 c = new Class1();
                var result = c.DetectNumbers(tempFile);
            }
        }
    }
}
