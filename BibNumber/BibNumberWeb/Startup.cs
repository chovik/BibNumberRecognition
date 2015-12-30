using Microsoft.Owin;
using MySql.Data.Entity;
using Owin;
using System.Data.Entity;

[assembly: OwinStartupAttribute(typeof(BibNumberWeb.Startup))]
namespace BibNumberWeb
{
    public partial class Startup
    {
        public void Configuration(IAppBuilder app)
        {
            ConfigureAuth(app);
            DbConfiguration.SetConfiguration(new MySqlEFConfiguration());
        }
    }
}
