
-- --------------------------------------------------
-- Entity Designer DDL Script for SQL Server 2005, 2008, 2012 and Azure
-- --------------------------------------------------
-- Date Created: 01/09/2016 20:17:12
-- Generated from EDMX file: F:\Projects\BibNumberRecognition\BibNumber\BibNumberWeb\PhotosModel.edmx
-- --------------------------------------------------

SET QUOTED_IDENTIFIER OFF;
GO
USE [bibnumber];
GO
IF SCHEMA_ID(N'dbo') IS NULL EXECUTE(N'CREATE SCHEMA [dbo]');
GO

-- --------------------------------------------------
-- Dropping existing FOREIGN KEY constraints
-- --------------------------------------------------

IF OBJECT_ID(N'[dbo].[FK_PhotoAlbumPhoto]', 'F') IS NOT NULL
    ALTER TABLE [dbo].[PhotoSet] DROP CONSTRAINT [FK_PhotoAlbumPhoto];
GO

-- --------------------------------------------------
-- Dropping existing tables
-- --------------------------------------------------

IF OBJECT_ID(N'[dbo].[PhotoSet]', 'U') IS NOT NULL
    DROP TABLE [dbo].[PhotoSet];
GO
IF OBJECT_ID(N'[dbo].[PhotoAlbumSet]', 'U') IS NOT NULL
    DROP TABLE [dbo].[PhotoAlbumSet];
GO

-- --------------------------------------------------
-- Creating all tables
-- --------------------------------------------------

-- Creating table 'PhotoSet'
CREATE TABLE [dbo].[PhotoSet] (
    [Id] int IDENTITY(1,1) NOT NULL,
    [Url] nvarchar(max)  NOT NULL,
    [PhotoAlbumId] int  NOT NULL,
    [ThumbnailUrl] nvarchar(max)  NOT NULL,
    [BibNumbersAsString] nvarchar(max)  NULL
);
GO

-- Creating table 'PhotoAlbumSet'
CREATE TABLE [dbo].[PhotoAlbumSet] (
    [Id] int IDENTITY(1,1) NOT NULL,
    [Url] nvarchar(max)  NOT NULL,
    [Name] nvarchar(max)  NOT NULL,
    [DetectionProgress] int  NOT NULL
);
GO

-- --------------------------------------------------
-- Creating all PRIMARY KEY constraints
-- --------------------------------------------------

-- Creating primary key on [Id] in table 'PhotoSet'
ALTER TABLE [dbo].[PhotoSet]
ADD CONSTRAINT [PK_PhotoSet]
    PRIMARY KEY CLUSTERED ([Id] ASC);
GO

-- Creating primary key on [Id] in table 'PhotoAlbumSet'
ALTER TABLE [dbo].[PhotoAlbumSet]
ADD CONSTRAINT [PK_PhotoAlbumSet]
    PRIMARY KEY CLUSTERED ([Id] ASC);
GO

-- --------------------------------------------------
-- Creating all FOREIGN KEY constraints
-- --------------------------------------------------

-- Creating foreign key on [PhotoAlbumId] in table 'PhotoSet'
ALTER TABLE [dbo].[PhotoSet]
ADD CONSTRAINT [FK_PhotoAlbumPhoto]
    FOREIGN KEY ([PhotoAlbumId])
    REFERENCES [dbo].[PhotoAlbumSet]
        ([Id])
    ON DELETE NO ACTION ON UPDATE NO ACTION;
GO

-- Creating non-clustered index for FOREIGN KEY 'FK_PhotoAlbumPhoto'
CREATE INDEX [IX_FK_PhotoAlbumPhoto]
ON [dbo].[PhotoSet]
    ([PhotoAlbumId]);
GO

-- --------------------------------------------------
-- Script has ended
-- --------------------------------------------------