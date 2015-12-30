using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;

namespace PhotoProvider
{
    /// <summary>
    /// Provides static methods for sending POST requests to the service Rajce.net.
    /// </summary>
    public class RequestManager
    {
        /// <summary>
        /// Sends GET requests to the specific url and returns content received from the server.
        /// </summary>
        /// <param name="url">url where the request is sent</param>
        /// <returns>content of the server response</returns>
        public static async Task<string> Get(string url)
        {
            string responseData = null;

            using (HttpClient client = new HttpClient())
            {
                try
                {
                    var response = await client.GetAsync(url);

                    if (response != null
                        && response.IsSuccessStatusCode)
                    {
                        string responseString = await response.Content.ReadAsStringAsync();

                        if (!string.IsNullOrWhiteSpace(responseString))
                        {
                            responseData = responseString;
                        }
                    }
                }
                catch (Exception ex)
                {

                }
            }

            return responseData;
        }
    }

        /// <summary>
        /// Sends specific data in POST request to the API of the service Rajce.net.
        /// </summary>
        /// <returns>the string (wrapped in the Task class, because method is performed as asynchronous operation) representing server response.</returns>
       
}
