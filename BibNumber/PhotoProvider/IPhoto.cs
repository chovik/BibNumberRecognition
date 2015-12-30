using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PhotoProvider
{
    public interface IPhoto
    {
        string Url { get; set; }
        string ThumbnailUrl { get; set; }
    }
}
