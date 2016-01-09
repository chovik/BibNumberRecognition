using System.Collections.Generic;
using System.Linq;

namespace Website
{
    public class ConnectionMapping<T>
    {
        private readonly Dictionary<T, HashSet<string>> connectionStore =
            new Dictionary<T, HashSet<string>>();

        public int Count
        {
            get { return connectionStore.Count; }
        }

        public void Add(T key, string connectionId)
        {
            lock (connectionStore)
            {
                HashSet<string> connections;
                if (!connectionStore.TryGetValue(key, out connections))
                {
                    connections = new HashSet<string>();
                    connectionStore.Add(key, connections);
                }

                lock (connections)
                {
                    connections.Add(connectionId);
                }
            }
        }

        public IEnumerable<string> GetConnections(T key)
        {
            lock (connectionStore)
            {
                HashSet<string> connections;
                if (connectionStore.TryGetValue(key, out connections))
                {
                    return connections;
                }
            }

            return Enumerable.Empty<string>();
        }

        public void Remove(T key, string connectionId)
        {
            lock (connectionStore)
            {
                HashSet<string> connections;
                if (!connectionStore.TryGetValue(key, out connections))
                {
                    return;
                }

                lock (connections)
                {
                    connections.Remove(connectionId);

                    if (connections.Count == 0)
                    {
                        connectionStore.Remove(key);
                    }
                }
            }
        }
    }
}