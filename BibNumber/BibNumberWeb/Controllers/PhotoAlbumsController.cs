using System;
using System.Collections.Generic;
using System.Data;
using System.Data.Entity;
using System.Linq;
using System.Threading.Tasks;
using System.Net;
using System.Web;
using System.Web.Mvc;
using PhotoProvider;
using System.Diagnostics;
using System.Net.Http;
using System.IO;
using Microsoft.WindowsAzure.Storage;
using Microsoft.WindowsAzure.Storage.Queue;
using System.Configuration;
using Newtonsoft.Json;
using Website;
using Microsoft.AspNet.SignalR;

namespace BibNumberWeb.Controllers
{
    /// <summary>
    /// Provides methods that respond to HTTP requests that are made to the site with url prefix '/PhotoAlbums/'.
    /// </summary>
    public class PhotoAlbumsController : Controller
    {
        private BibNumbersMysqlContext db = new BibNumbersMysqlContext();

        /// <summary>
        /// GET: PhotoAlbums
        /// Load list of albums from a database and navigates to the site where the list is displayed.
        /// </summary>
        /// <returns></returns>
        public async Task<ActionResult> Index()
        {
            return View(await db.PhotoAlbumSet.ToListAsync());
        }

        /// <summary>
        /// GET: PhotoAlbums/Details/5
        /// Loads an album with an specific id and navigates to the site displaying details of the requested album.
        /// </summary>
        /// <param name="id">id of an album to load</param>
        /// <returns>
        /// Bad Request result, if the passed id is null.
        /// Not Found object, if the requested album was not found.
        /// View that is used to render details page, if the requested album was found.
        /// </returns>
        public async Task<ActionResult> Details(int? id)
        {
            if (id == null)
            {
                return new HttpStatusCodeResult(HttpStatusCode.BadRequest);
            }
            PhotoAlbum photoAlbum = await db.PhotoAlbumSet.FindAsync(id);
            if (photoAlbum == null)
            {
                return HttpNotFound();
            }
            return View(photoAlbum);
        }

        /// <summary>
        /// GET: PhotoAlbums/Create
        /// Creates a view that is used to create new album.
        /// </summary>
        /// <returns>View that is used to create new album.</returns>
        public ActionResult Create()
        {
            return View();
        }

        /// <summary>
        /// POST: PhotoAlbums/Create
        /// Handles POST request to create new album. Binds Url and Name from the POST data to the photoAlbum parameter.
        /// Checks if the form data are valid and loads photos from the filled url. 
        /// Method is now working only with urls that links to the albums from the rajce.net site.
        /// If the form data are valid, photos from the rajce.net are loaded and new album is created and stored to the database.
        /// After the album is stored to the database, method informs Azure WebJob service to run detection algorithm on the loaded photos.
        /// To protect from overposting attacks, please enable the specific properties you want to bind to, for 
        /// more details see http://go.microsoft.com/fwlink/?LinkId=317598.
        /// </summary>
        /// <param name="photoAlbum">album that contains Url and Name passed as POST data</param>
        /// <returns></returns>
        [HttpPost]
        [ValidateAntiForgeryToken]
        public async Task<ActionResult> Create([Bind(Include = "Url,Name")] PhotoAlbum photoAlbum)
        {
            try
            {
                if (ModelState.IsValid)
                {
                    RajcePhotoProvider photoProvider = new RajcePhotoProvider();
                    var photoList = await photoProvider.GetPhotoList(photoAlbum.Url);

                    if (photoList != null
                        && photoList.Count() > 0)
                    {
                        foreach (var photo in photoList)
                        {
                            var photoModel = new Photo()
                            {
                                Url = photo.Url,
                                ThumbnailUrl = photo.ThumbnailUrl
                            };

                            photoAlbum.Photos.Add(photoModel);

                        }
                    }

                    db.PhotoAlbumSet.Add(photoAlbum);
                    await db.SaveChangesAsync();

                    await DetectBibNumbers(photoAlbum);

                    //foreach(var photo in photoAlbum.Photos)
                    //{
                    //    await DetectBibNumbers(photo);
                    //}

                    return Redirect("~/PhotoAlbums/Details/" + photoAlbum.Id);
                }
            }
            catch(Exception ex)
            {

            }
            

            return View(photoAlbum);
        }

        /// <summary>
        /// GET: PhotoAlbums/Edit/5
        /// </summary>
        /// <param name="id"></param>
        /// <returns></returns>
        public async Task<ActionResult> Edit(int? id)
        {
            if (id == null)
            {
                return new HttpStatusCodeResult(HttpStatusCode.BadRequest);
            }
            PhotoAlbum photoAlbum = await db.PhotoAlbumSet.FindAsync(id);
            if (photoAlbum == null)
            {
                return HttpNotFound();
            }
            return View(photoAlbum);
        }

        // POST: PhotoAlbums/Edit/5
        // To protect from overposting attacks, please enable the specific properties you want to bind to, for 
        // more details see http://go.microsoft.com/fwlink/?LinkId=317598.
        [HttpPost]
        [ValidateAntiForgeryToken]
        public async Task<ActionResult> Edit([Bind(Include = "Id,Url,Name")] PhotoAlbum photoAlbum)
        {
            if (ModelState.IsValid)
            {
                db.Entry(photoAlbum).State = EntityState.Modified;
                await db.SaveChangesAsync();
                return RedirectToAction("Index");
            }
            return View(photoAlbum);
        }

        // GET: PhotoAlbums/Delete/5
        public async Task<ActionResult> Delete(int? id)
        {
            if (id == null)
            {
                return new HttpStatusCodeResult(HttpStatusCode.BadRequest);
            }
            PhotoAlbum photoAlbum = await db.PhotoAlbumSet.FindAsync(id);
            if (photoAlbum == null)
            {
                return HttpNotFound();
            }
            return View(photoAlbum);
        }

        // POST: PhotoAlbums/Delete/5
        [HttpPost, ActionName("Delete")]
        [ValidateAntiForgeryToken]
        public async Task<ActionResult> DeleteConfirmed(int id)
        {
            PhotoAlbum photoAlbum = await db.PhotoAlbumSet.FindAsync(id);
            db.PhotoAlbumSet.Remove(photoAlbum);
            await db.SaveChangesAsync();
            return RedirectToAction("Index");
        }

        public async Task<ActionResult> Search(int id, FormCollection form)
        {
            var query = form["SearchQuery"];

            if(string.IsNullOrWhiteSpace(query))
            {
                return HttpNotFound();
            }

            int bibNumber = -1;

            if(!int.TryParse(query, out bibNumber))
            {
                return HttpNotFound();
            }

            PhotoAlbum photoAlbum = await db.PhotoAlbumSet.FindAsync(id);

            if (photoAlbum == null)
            {
                return HttpNotFound();
            }

            PhotoAlbum resultsAlbum = new PhotoAlbum();
            var results = photoAlbum.Photos.Where(p => p.BibNumbersAsString.Contains(query)).ToList();
            resultsAlbum.Photos = results;

            TempData["SearchQuery"] = query;
            return View(resultsAlbum);
        }

        public async Task DetectBibNumbers(Photo photo)
        {
            try
            {
                CloudStorageAccount storageAccount = CloudStorageAccount.Parse(ConfigurationManager.ConnectionStrings["AzureWebJobsStorage"].ConnectionString);//CloudStorageAccount.Parse("UseDevelopmentStorage=true");
                CloudQueueClient queueClient = storageAccount.CreateCloudQueueClient();
                CloudQueue queue = queueClient.GetQueueReference("detectbibnumbersqueue");
                queue.CreateIfNotExists();
                var photoJson = await JsonConvert.SerializeObjectAsync(photo);
                queue.AddMessage(new CloudQueueMessage(photoJson));
            }
            catch (Exception ex)
            {

            }            
        }

        public async Task DetectBibNumbers(PhotoAlbum album)
        {
            try
            {
                var connectionString = ConfigurationManager.ConnectionStrings["AzureWebJobsStorage"].ConnectionString;
                CloudStorageAccount storageAccount = CloudStorageAccount.Parse(connectionString);//CloudStorageAccount.Parse("UseDevelopmentStorage=true");tionManager.ConnectionStrings["AzureStorageConnection"].ConnectionString);//CloudStorageAccount.Parse("UseDevelopmentStorage=true");
                CloudQueueClient queueClient = storageAccount.CreateCloudQueueClient();
                CloudQueue queue = queueClient.GetQueueReference("detectbibnumbersqueue");
                queue.CreateIfNotExists();
                var albumJson = await JsonConvert.SerializeObjectAsync(album);
                queue.AddMessage(new CloudQueueMessage(albumJson));
            }
            catch (Exception ex)
            {

            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                db.Dispose();
            }
            base.Dispose(disposing);
        }

        public async Task<ActionResult> ProgressNotification(int jobId, int progress)
        {
            var connections = JobProgressHub.GetUserConnections(jobId);

            if (connections != null)
            {
                foreach (var connection in connections)
                {
                    // Notify the client to refresh the list of connections
                    var hubContext = GlobalHost.ConnectionManager.GetHubContext<JobProgressHub>();
                    hubContext.Clients.Clients(new[] { connection }).updateProgress(progress);
                }
            }

            return new HttpStatusCodeResult(HttpStatusCode.OK);
        }
    }
}
