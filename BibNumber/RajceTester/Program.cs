using PhotoProvider;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RajceTester
{
    class Program
    {
        private static async Task Test()
        {
            RajcePhotoProvider photoProvider = new RajcePhotoProvider();
            var photos = await photoProvider.GetPhotoList("http://doki.rajce.idnes.cz/MMM_2015/");
        }
        static void Main(string[] args)
        {
            Test().Wait();
        }
    }
}
