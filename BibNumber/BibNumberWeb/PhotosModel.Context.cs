﻿//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated from a template.
//
//     Manual changes to this file may cause unexpected behavior in your application.
//     Manual changes to this file will be overwritten if the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace BibNumberWeb
{
    using System;
    using System.Data.Entity;
    using System.Data.Entity.Infrastructure;
    
    public partial class BibNumbersMysqlContext : DbContext
    {
        public BibNumbersMysqlContext()
            : base("name=BibNumbersMysqlContext")
        {
        }
    
        protected override void OnModelCreating(DbModelBuilder modelBuilder)
        {
            //throw new UnintentionalCodeFirstException();
        }
    
        public virtual DbSet<Photo> PhotoSet { get; set; }
        public virtual DbSet<PhotoAlbum> PhotoAlbumSet { get; set; }
    }
}
