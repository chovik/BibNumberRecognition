using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Azure.WebJobs;
using Microsoft.WindowsAzure.Storage;
using Microsoft.WindowsAzure;
using BibNumberWeb;
using System.Data.Entity;
using MySql.Data.Entity;
using BibNumberWrapper;
using Newtonsoft.Json;
using System.IO;
using System.Net;

namespace BibNumbersDetectionWebJob
{
    // To learn more about Microsoft Azure WebJobs SDK, please see http://go.microsoft.com/fwlink/?LinkID=320976
    class Program
    {
        private static BibNumbersMysqlContext db = new BibNumbersMysqlContext();
        // Please set the following connection strings in app.config for this WebJob to run:
        // AzureWebJobsDashboard and AzureWebJobsStorage
        static void Main()
        {
#if DEBUG
            //DbConfiguration.SetConfiguration(new MySqlEFConfiguration());
            while(true)
            {
                GetJobsFromQueue();
            }
            
#else
            var host = new JobHost();
            host.RunAndBlock();
#endif
        }
        
        private static void GetJobsFromQueue()
        {
            //Log("Getting Load jobs from queue...");
            var storageAccount = CloudStorageAccount.Parse("UseDevelopmentStorage=true");
            var queueClient = storageAccount.CreateCloudQueueClient();
            var queue = queueClient.GetQueueReference("detectbibnumbersqueue");
            var retrievedMessage = queue.GetMessage();

            if (retrievedMessage == null)
            {
                return;
            }

            //var album = db.PhotoSet.Find(1);
            

            //Log($"Retrieved message with content '{retrievedMessage.AsString}'");
            Functions.ProcessQueueMessage(retrievedMessage.AsString);
            queue.DeleteMessage(retrievedMessage);
            //Log("Deleted message");
        }
    }
}
