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
using System.Data.Entity;
using System.Net.Http;

namespace BibNumbersDetectionWebJob
{
    public class Functions
    {
        //private static BibNumbersMysqlContext db = new BibNumbersMysqlContext();
        // This function will get triggered/executed when a new message is written 
        // on an Azure Queue called queue.
        //public async static Task ProcessPhotoMessage([QueueTrigger("detectbibnumbersqueue")] string message)
        //{
        //    var photo = JsonConvert.DeserializeObject<Photo>(message);
        //    ProcessPhoto(photo);
            
        //}

        public async static Task ProcessAlbumMessage([QueueTrigger("detectbibnumbersqueue")] string message)
        {
            var album = JsonConvert.DeserializeObject<PhotoAlbum>(message);

            if(album == null)
            {
                return;
            }

            ProcessAlbum(album);
            
        }

        public async static void ProcessAlbum(PhotoAlbum album)
        {
            using (BibNumbersMysqlContext db = new BibNumbersMysqlContext())
            {
                var foundAlbum = db.PhotoAlbumSet.FirstOrDefault(a => a.Id == album.Id);

                if (foundAlbum != null)
                {
                    var photosCount = foundAlbum.Photos.Count;
                    foundAlbum.DetectionProgress = 0;
                    db.Entry(foundAlbum).State = EntityState.Modified;
                    db.SaveChanges();
                    await CommunicateProgress(album.Id, 0);

                    for (int photoIndex = 0; photoIndex < photosCount; photoIndex++)
                    {
                        int percentage = ((photoIndex + 1) * 100) / photosCount;
                        foundAlbum.DetectionProgress = percentage;
                        db.Entry(foundAlbum).State = EntityState.Modified;
                        db.SaveChanges();
                        var photo = foundAlbum.Photos.ElementAt(photoIndex);
                        ProcessPhoto(photo, db);
                        await CommunicateProgress(foundAlbum.Id, percentage);
                    }

                    album.DetectionProgress = 100;
                    db.Entry(foundAlbum).State = EntityState.Modified;
                    db.SaveChanges();
                }
            }
                    
        }

        public static void ProcessPhoto(Photo photo, BibNumbersMysqlContext db)
        {
            var tempFile = Path.GetTempFileName();
            using (WebClient webClient = new WebClient())
            {
                webClient.DownloadFile(photo.Url, tempFile);
                Class1 c = new Class1();
                var foundBibNumbers = c.DetectNumbers(tempFile);

                if (foundBibNumbers != null
                    && foundBibNumbers.Count > 0)
                {
                    foreach (var number in foundBibNumbers)
                    {
                        photo.BibNumbers.Add(number.ToString());
                    }

                    db.Entry(photo).State = EntityState.Modified;
                    db.SaveChanges();
                }
            }
        }

        private static async Task CommunicateProgress(int jobId, int percentage)
        {
            var httpClient = new HttpClient();

            var queryString = String.Format("?jobId={0}&progress={1}", jobId, percentage);
            var request = "http://localhost:2221/PhotoAlbums/ProgressNotification" + queryString;

            await httpClient.GetAsync(request);
        }
    }
}
