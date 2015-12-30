﻿using System;
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

namespace BibNumberWeb.Controllers
{
    public class PhotoAlbumsController : Controller
    {
        private BibNumbersMysqlContext db = new BibNumbersMysqlContext();

        // GET: PhotoAlbums
        public async Task<ActionResult> Index()
        {
            return View(await db.PhotoAlbumSet.ToListAsync());
        }

        // GET: PhotoAlbums/Details/5
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

        // GET: PhotoAlbums/Create
        public ActionResult Create()
        {
            return View();
        }

        // POST: PhotoAlbums/Create
        // To protect from overposting attacks, please enable the specific properties you want to bind to, for 
        // more details see http://go.microsoft.com/fwlink/?LinkId=317598.
        [HttpPost]
        [ValidateAntiForgeryToken]
        public async Task<ActionResult> Create([Bind(Include = "Id,Url,Name")] PhotoAlbum photoAlbum)
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

                        

                        photoModel.BibNumbers.Add("212");
                        photoModel.BibNumbers.Add("411");
                        photoModel.BibNumbers.Add("392");

                        photoAlbum.Photos.Add(photoModel);

                        DetectBibNumbers(photoModel);
                        
                    }
                }

                db.PhotoAlbumSet.Add(photoAlbum);
                await db.SaveChangesAsync();
                return RedirectToAction("Index");
            }

            return View(photoAlbum);
        }

        // GET: PhotoAlbums/Edit/5
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
            //Task.Run(() =>
            //    {
                    try
                    {
                        CloudStorageAccount storageAccount = CloudStorageAccount.Parse(ConfigurationManager.ConnectionStrings["AzureStorageConnection"].ConnectionString);
                        CloudQueueClient queueClient = storageAccount.CreateCloudQueueClient();
                        CloudQueue queue = queueClient.GetQueueReference("detectbibnumbersqueue");
                        queue.CreateIfNotExists();
                        var photoJson = await JsonConvert.SerializeObjectAsync(photo);
                        queue.AddMessage(new CloudQueueMessage(photoJson));
                        using(WebClient webClient = new WebClient())
                        {
                            var tempFile = Path.GetTempFileName();
                            webClient.DownloadFile(photo.Url, tempFile);
                            //BibNumberWrapper.Class1 c = new BibNumberWrapper.Class1();
                            //double result = c.DetectNumbers(tempFile);
                            //var d = result;
                        }
                    }
                    catch (Exception ex)
                    {

                    }
                //});
            
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                db.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}