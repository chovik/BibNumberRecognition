using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PhotoProvider
{
    public class Photo : IPhoto
    {
        public string Url
        {
            get;
            set;
        }

        public string ThumbnailUrl
        {
            get;
            set;
        }
    }
}
