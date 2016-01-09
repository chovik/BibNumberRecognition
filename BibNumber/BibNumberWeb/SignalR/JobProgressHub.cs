using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNet.SignalR;

namespace Website
{
    public class JobProgressHub : Hub
    {
        private readonly static ConnectionMapping<int> Connections = new ConnectionMapping<int>();

        public override Task OnConnected()
        {
            var jobid = GetJobId();

            Connections.Add(jobid, Context.ConnectionId);

            return base.OnConnected();
        }

        public override Task OnDisconnected(bool stopCalled)
        {
            var jobid = GetJobId();

            Connections.Remove(jobid, Context.ConnectionId);

            return base.OnDisconnected(stopCalled);
        }

        public override Task OnReconnected()
        {
            var jobId = GetJobId();

            if (!Connections.GetConnections(jobId).Contains(Context.ConnectionId))
            {
                Connections.Add(jobId, Context.ConnectionId);
            }

            return base.OnReconnected();
        }

        private int GetJobId()
        {
            int jobId = -1;
            var strId = Context.QueryString["jobId"];
            int.TryParse(strId, out jobId);
            return jobId;
        }

        public static IEnumerable<string> GetUserConnections(int jobId)
        {
            return Connections.GetConnections(jobId);
        }
    }
}