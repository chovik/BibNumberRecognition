using HtmlAgilityPack;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace PhotoProvider
{
    public class RajcePhotoProvider : IPhotoProvider
    {
        public async Task<IEnumerable<IPhoto>> GetPhotoList(string url)
        {
            string[] photosUrls = null;
            List<IPhoto> photos = null;

            var html = new HtmlWeb().Load(url);

            if (html.DocumentNode != null)
            {
                string htmlString = html.DocumentNode.InnerHtml;

                if (!string.IsNullOrWhiteSpace(htmlString))
                {
                    var photoIdPatter = ".*photoID\\s*:\\s*\"([^\"]*)\".*";
                    var photoIdMatches = Regex.Matches(htmlString, photoIdPatter);
                    List<string> photoIds = new List<string>();

                    if(photoIdMatches != null
                        && photoIdMatches.Count > 0)
                    {
                        foreach(Match photoIdMatch in photoIdMatches)
                        {
                            if(photoIdMatch.Groups.Count == 2)
                            {
                                var photoId = photoIdMatch.Groups[1].Value;
                                photoIds.Add(photoId);
                            }
                        }
                    }

                    var storagePattern = ".*var storage = \"([^\"]*)\";.*";
                    var storageMatch = Regex.Match(htmlString, storagePattern);

                    if (storageMatch != null
                        && storageMatch.Success
                        && storageMatch.Groups.Count == 2)
                    {
                        var storageUrl = storageMatch.Groups[1].Value;

                        photosUrls = html.DocumentNode.Descendants("a")
                                      .Select(a => a.GetAttributeValue("href", null))
                                      .Where(u => !String.IsNullOrEmpty(u) && u.StartsWith(storageUrl)).ToArray();

                    }

                    photos = new List<IPhoto>();
                    bool includeThumbnail = photoIds.Count == photosUrls.Count();
  
                    for(int photoIndex = 0; photoIndex < photosUrls.Count(); photoIndex++)
                    {
                        var photoUrl = photosUrls[photoIndex];

                        IPhoto photo = new Photo()
                        {
                            Url = photoUrl
                        };

                        if (includeThumbnail)
                        {
                            var photoId = photoIds[photoIndex];
                            photo.ThumbnailUrl = "http://www.rajce.idnes.cz/f" + photoId + "/res/300.jpg";
                        }

                        photos.Add(photo);
                    }

                }
            }

            return photos;
        }
    }
}
